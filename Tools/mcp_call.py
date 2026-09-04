"""
Talk to the editor's MCP server directly over HTTP.

WHY THIS EXISTS
---------------
Claude Code's MCP client connects once, at session start. If the editor was
closed at that moment - which it always is when a session begins with a build -
the connection is refused and the `unreal-mcp` tools stay unavailable for the
WHOLE session, even after the editor comes back up and port 8000 starts
listening. CLAUDE.md already notes MCP "needs reconnecting after every editor
restart"; this is the fallback for when that reconnect cannot be triggered from
inside the session.

The server is a plain JSON-RPC-over-HTTP endpoint, so nothing here is a
workaround so much as the same transport spoken by hand.

USAGE
-----
    python Tools/mcp_call.py list
    python Tools/mcp_call.py describe EditorToolset
    python Tools/mcp_call.py call <toolset> <tool> '<json args>'

    # e.g.
    python Tools/mcp_call.py call ActorTools find_actors '{"pattern": "*Bar*"}'

Batch several calls in one process (much faster - one handshake, not N):

    python Tools/mcp_call.py batch calls.json

where calls.json is [{"toolset": "...", "tool": "...", "args": {...}}, ...]
"""

import json
import sys
import urllib.request

URL = "http://127.0.0.1:8000/mcp"

# The registry wants FULLY QUALIFIED toolset names ("editor_toolset.toolsets.
# scene.SceneTools"), not the short ones every doc and habit uses. Passing
# "SceneTools" fails with "Toolset 'SceneTools' not found", which reads like the
# plugin is missing rather than like a naming problem - so short names are
# expanded here instead of being learned the hard way each time.
TOOLSET_ALIASES = {
    "ActorTools":            "editor_toolset.toolsets.actor.ActorTools",
    "AssetTools":            "editor_toolset.toolsets.asset.AssetTools",
    "BlueprintTools":        "editor_toolset.toolsets.blueprint.BlueprintTools",
    "CurveTableTools":       "editor_toolset.toolsets.curve_table.CurveTableTools",
    "DataAssetTools":        "editor_toolset.toolsets.data_asset.DataAssetTools",
    "DataTableTools":        "editor_toolset.toolsets.data_table.DataTableTools",
    "MaterialTools":         "editor_toolset.toolsets.material.MaterialTools",
    "MaterialInstanceTools": "editor_toolset.toolsets.material_instance.MaterialInstanceTools",
    "ObjectTools":           "editor_toolset.toolsets.object.ObjectTools",
    "PrimitiveTools":        "editor_toolset.toolsets.primitive.PrimitiveTools",
    "SceneTools":            "editor_toolset.toolsets.scene.SceneTools",
    "SkeletalMeshTools":     "editor_toolset.toolsets.skeletal_mesh.SkeletalMeshTools",
    "StaticMeshTools":       "editor_toolset.toolsets.static_mesh.StaticMeshTools",
    "StringTableTools":      "editor_toolset.toolsets.string_table.StringTableTools",
    "ProgrammaticToolset":   "editor_toolset.toolsets.programmatic.ProgrammaticToolset",
    "TextureTools":          "editor_toolset.toolsets.texture.TextureTools",
    "BehaviorTreeTools":     "aimodule_toolset.toolsets.behavior_tree.BehaviorTreeTools",
    "EditorAppToolset":      "EditorToolset.EditorAppToolset",
    "LogsToolset":           "EditorToolset.LogsToolset",
    "UMGToolSet":            "UMGToolSet.UMGToolSet",
    "GameplayTagsToolset":   "GameplayTagsToolset.GameplayTagsToolset",
    "LiveCodingToolset":     "LiveCodingToolset.LiveCodingToolset",
}


class McpSession:
    def __init__(self, url=URL):
        self.url = url
        self.session_id = None
        self.next_id = 1
        self._initialize()

    def _post(self, payload, notify=False):
        data = json.dumps(payload).encode("utf-8")
        headers = {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
        }
        if self.session_id:
            headers["Mcp-Session-Id"] = self.session_id

        req = urllib.request.Request(self.url, data=data, headers=headers, method="POST")
        with urllib.request.urlopen(req, timeout=120) as resp:
            if self.session_id is None:
                self.session_id = resp.headers.get("Mcp-Session-Id")
            body = resp.read().decode("utf-8", errors="replace")

        if notify or not body.strip():
            return None

        # The server may answer as plain JSON or as an SSE frame depending on
        # what it feels like; accept either rather than assuming.
        if body.lstrip().startswith("event:") or body.lstrip().startswith("data:"):
            for line in body.splitlines():
                if line.startswith("data:"):
                    body = line[len("data:"):].strip()
                    break
        return json.loads(body)

    def _rpc(self, method, params=None):
        payload = {"jsonrpc": "2.0", "id": self.next_id, "method": method}
        if params is not None:
            payload["params"] = params
        self.next_id += 1
        return self._post(payload)

    def _initialize(self):
        self._rpc("initialize", {
            "protocolVersion": "2024-11-05",
            "capabilities": {},
            "clientInfo": {"name": "claude-code-direct", "version": "1.0"},
        })
        # Required by the protocol before any tools/* call will be honoured.
        self._post({"jsonrpc": "2.0", "method": "notifications/initialized"}, notify=True)

    def list_tools(self):
        return self._rpc("tools/list")

    def call(self, name, arguments):
        return self._rpc("tools/call", {"name": name, "arguments": arguments})

    def call_tool(self, toolset, tool, arguments):
        """
        Toolset tools are not exposed individually - the server publishes three
        top-level tools (list_toolsets / describe_toolset / call_tool) and
        everything else is dispatched through call_tool. Same shape the
        in-process `mcp__unreal-mcp__call_tool` tool uses.
        """
        args = {"tool_name": tool, "arguments": arguments}
        if toolset:
            args["toolset_name"] = TOOLSET_ALIASES.get(toolset, toolset)
        return self.call("call_tool", args)


def unwrap(result):
    """Pull the useful text out of an MCP tool result envelope."""
    if not isinstance(result, dict):
        return result
    if "error" in result:
        return result["error"]
    content = result.get("result", {}).get("content")
    if isinstance(content, list):
        out = []
        for part in content:
            if part.get("type") == "text":
                out.append(part.get("text", ""))
            else:
                out.append(f"[{part.get('type')}]")
        return "\n".join(out)
    return result.get("result", result)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1

    mode = sys.argv[1]
    s = McpSession()

    if mode == "list":
        print(json.dumps(s.list_tools(), indent=1)[:20000])
        return 0

    if mode == "describe":
        name = TOOLSET_ALIASES.get(sys.argv[2], sys.argv[2])
        print(unwrap(s.call("describe_toolset", {"toolset_name": name})))
        return 0

    if mode == "toolsets":
        print(unwrap(s.call("list_toolsets", {})))
        return 0

    if mode == "call":
        toolset, tool = sys.argv[2], sys.argv[3]
        args = json.loads(sys.argv[4]) if len(sys.argv) > 4 else {}
        print(unwrap(s.call_tool(toolset, tool, args)))
        return 0

    if mode == "batch":
        calls = json.load(open(sys.argv[2], encoding="utf-8"))
        for i, c in enumerate(calls):
            print(f"--- [{i}] {c.get('toolset','')}.{c['tool']} ---")
            print(unwrap(s.call_tool(c.get("toolset"), c["tool"], c.get("args", {}))))
        return 0

    print(f"unknown mode: {mode}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
