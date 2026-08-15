# Connecting the local AI to the game

How Zoe's local LLM becomes the System AI's voice.

---

## The hard line, restated

The design doc is explicit and the tone bible repeats it: **the AI narrates outcomes
but never decides them.** Every number in this game is rolled by `UDCCRuleset` on the
server. The model's only job is to say what already happened, in character.

This is not only a correctness rule, it is the thing that makes the integration
*tractable*. A narrator that cannot affect state can be slow, can fail, can be
switched off, and can produce garbage — and none of those break the game. Every
design decision below falls out of that one constraint.

If the model is unreachable, the game plays exactly as it does today, with the
existing canned lines. **The narrator is a garnish, and it must be architected like
one.**

---

## Where it plugs in: the event log already is the interface

`ADCCGameState` already maintains a replicated, capped, human-readable feed of
everything the game decided — attacks, skills, rank-ups, deaths, cleared rooms,
favor swings, achievements, quests. It was built to make the dice visible to the
table.

It turns out to be exactly the right input for a narrator, and that is not a
coincidence: both want the same thing, which is a plain-language account of resolved
outcomes with no mechanics attached.

So the integration is small:

```
ADCCGameState::AddLogEntry(Category, Text)
        |
        +--> replicated feed (exists today, unchanged)
        |
        +--> UDCCNarratorComponent::Consider(Entry)     [server only]
                    |
                    | (throttled, batched, async)
                    v
              local LLM over HTTP
                    |
                    v
        ADCCGameState::Log(Announcement, <model output>)
```

The narrator reads the same feed the players read and writes back into it as an
`Announcement`. Nothing else in the codebase needs to know it exists.

---

## Transport

**HTTP POST to a local endpoint**, via `FHttpModule`, which ships with the engine
and needs no plugin.

Assuming Ollama, which is the usual local runner:

```
POST http://127.0.0.1:11434/api/generate
{
  "model":  "<whatever Zoe has pulled>",
  "prompt": "<system prompt + recent events>",
  "stream": false,
  "options": { "temperature": 0.8, "num_predict": 60 }
}
```

LM Studio exposes an OpenAI-compatible `/v1/chat/completions` on port 1234 instead.
**Both should be supported by making the URL and the request shape config-driven**,
because switching runners is a thing Zoe will do and it should not need a rebuild.

Settings belong in `DefaultGame.ini` under a `[/Script/DungeonCrawlerDenver.DCCNarratorSettings]`
section: endpoint URL, model name, temperature, token cap, timeout, and an
`bEnabled` master switch.

### Server only, always

The narrator runs on the server and its output reaches clients through the existing
replicated log. Five clients each calling a local model would mean five different
narrations of the same event, and four of them talking about a machine that is not
theirs.

---

## Timing: the part that will actually bite

A local model on a consumer GPU takes **roughly 0.5–3 seconds** for sixty tokens.
That is far too slow to narrate a sword swing, and fast enough to narrate a room.

So the narrator does **not** speak on every event. It has three modes:

| Mode | Trigger | Latency budget |
|---|---|---|
| **Beat** | A significant single event: a death, a boss phase, an achievement | Fire and forget; arrives when it arrives |
| **Recap** | An encounter clears, a floor clears, a quest completes | Up to ~3s is fine; there is a natural pause |
| **Idle colour** | 60s+ of nothing happening | Entirely optional, drop under load |

Ordinary hits and misses are **never** sent. They already have canned lines, they
happen several times a second, and a model commenting on each would be both
expensive and unbearable.

### Everything is fire-and-forget

`Consider()` returns immediately. The HTTP response arrives on a later frame and is
appended to the log with its own timestamp. **Nothing ever waits on the model** —
no gameplay code blocks, no RPC stalls, no loading screen.

If a response arrives after the moment has passed, it is dropped rather than shown
late. A narrator commenting on a fight that ended forty seconds ago is worse than
silence.

### One request in flight, ever

A hard rule, not a tuning knob. Local models serialise anyway, so a queue would only
build a backlog of stale narration. If a request is outstanding when a new beat
arrives, the new beat wins and the old request's response is discarded on receipt.

---

## The prompt

Two parts: a fixed system prompt carrying the voice, and a rolling window of the
last N log entries.

The system prompt is essentially `Docs/dungeon-tone-bible.md` §1 and §7 compressed —
the voice, plus the seven writing rules. §7's first rule is the important one and
should be stated twice, at the top and bottom:

> Never invent a mechanical outcome. You describe what the game has already
> decided. If you are unsure whether something happened, do not mention it.

**Hard constraints in the prompt:**
- One or two sentences. Never more. (Reinforced by `num_predict`.)
- Never address the player as "you the player"; the audience is watching crawlers.
- Never state a number that is not in the events given.
- Never invent a character name.

**Sanitising the output.** Model output goes into a replicated `FText` seen by five
people, so it gets checked before it is logged:

- Truncate at a hard character cap.
- Drop it entirely if it contains a digit not present in the source events — the
  cheapest possible guard against invented mechanics, and it directly enforces the
  design doc's line.
- Strip markdown, quotes and any leading "Narrator:" style prefix, which small
  models add constantly.

That digit check is worth the trouble. It is the difference between a narrator that
is charming and one that tells the party they took 14 damage when they took 6.

---

## Failure behaviour

| Failure | Response |
|---|---|
| Endpoint unreachable | Log once at `Warning`, disable for 60s, retry. Never spam |
| Timeout | Discard, carry on |
| Empty or refused output | Discard silently |
| Output fails sanitising | Discard silently, count it |
| `bEnabled=false` | Component never ticks; zero cost |

**Silence is always an acceptable output.** The game shipped without a narrator and
still works without one.

---

## What to build, in order

1. **`Progression/`-style new folder `Narration/`** with `UDCCNarratorSettings`
   (a `UDeveloperSettings` so it appears in Project Settings) and
   `UDCCNarratorComponent` on `ADCCGameState`.
2. **The throttle and mode logic first, with a stub that returns a canned string.**
   This is testable without a model running and shakes out the timing, which is
   where the real risk is.
3. **The HTTP call**, behind the config.
4. **The sanitiser**, with the digit guard.
5. **Only then** tune the prompt, which is the part that is fun and the part that
   matters least until 1–4 are solid.

Steps 1–2 are worth doing even if the model integration is deferred, because a
narrator component that picks *which moments deserve comment* is useful with canned
lines too — and that selection logic is the genuinely hard design problem here, not
the HTTP.

---

## What this deliberately does not do

- **No dialogue generation for NPCs.** The bartender's lines are authored in
  `DT_DCCDialogue.json` because they carry plot and need to be consistent across
  sessions. A model improvising them would contradict itself between playthroughs.
- **No enemy behaviour.** Nothing about targeting, difficulty or spawning.
- **No quest generation.** Quests pay XP.
- **No voice synthesis.** Text only, for now. TTS is a separate problem with its own
  latency budget and should not be tangled into this one.
