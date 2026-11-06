// The crawler's 3D model, playing clips from the game's own source art.
//
// THE RETARGETING RULE (and why the in-game crawlers looked stretched)
//
// All four Meshy rigs share the same 24 bone names, so a clip made on one body can drive another. But every
// Meshy clip keys bone TRANSLATION as well as rotation, and a translation key is a bone length: play CJ's run
// on Marcel's body and Marcel's arms become CJ's arms. So clips borrowed from another crawler are applied
// rotation-only, with just the Hips' movement kept and scaled to this body's hip height. A crawler's own clips
// are used untouched. The UE side gets the same fix through the skeleton's translation-retargeting modes
// (Source/DungeonCrawlerDenver/Characters/DCCRetargetFix.cpp).

import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';

const loader = new GLTFLoader();
const cache = new Map();

function loadGltf(url) {
  // Rest pose is captured once, at parse time: the cached scene's bones are animated as soon as anything plays it.
  if (!cache.has(url)) {
    cache.set(url, loader.loadAsync(url).then((gltf) => {
      const pose = bindPose(gltf.scene);
      gltf.userData.rest = pose.rest;
      gltf.userData.hipsY = pose.hipsY;
      return gltf;
    }));
  }
  return cache.get(url);
}

function findHips(root) {
  let hips = null;
  root.traverse((o) => {
    if (!hips && o.isBone && /hips/i.test(o.name)) hips = o;
  });
  return hips;
}

/**
 * The bind pose, from the skin's inverse-bind matrices.
 *
 * NOT the bones' node transforms: in Meshy's merged-animation .glb those hold the first frame of some clip,
 * which differs between files by as much as 130 degrees at the hip. Only the inverse-bind matrices describe
 * the pose the mesh was actually skinned in. Returns local rest rotations by bone name plus the Hips height.
 */
function bindPose(root) {
  const rest = new Map();
  let hipsY = 1;
  let skinned = null;
  root.traverse((o) => {
    if (!skinned && o.isSkinnedMesh) skinned = o;
  });
  if (!skinned) return { rest, hipsY };

  const { bones, boneInverses } = skinned.skeleton;
  const world = boneInverses.map((inv) => inv.clone().invert());
  const indexOf = new Map(bones.map((b, i) => [b, i]));
  const local = new THREE.Matrix4();
  const pos = new THREE.Vector3();
  const quat = new THREE.Quaternion();
  const scl = new THREE.Vector3();

  bones.forEach((bone, i) => {
    const parentIndex = indexOf.get(bone.parent);
    if (parentIndex !== undefined) local.copy(world[parentIndex]).invert().multiply(world[i]);
    else local.copy(world[i]);
    local.decompose(pos, quat, scl);
    rest.set(bone.name, quat.clone());
    if (/hips/i.test(bone.name)) {
      world[i].decompose(pos, quat, scl);
      hipsY = pos.y || 1;
    }
  });
  return { rest, hipsY };
}

/**
 * Rotation-only copy of a clip, with Hips translation scaled to the target's proportions.
 *
 * Rotations are carried over relative to each rig's rest pose, not copied raw: Meshy fits a fresh skeleton to
 * every character, so the same-named bone can rest at a slightly different angle on each body. Copying the raw
 * key would bake the source's rest angle into the target (the raised knee and splayed arms of a naive copy).
 * target = targetRest * inverse(sourceRest) * sourceKey keeps only the motion.
 */
function retargetClip(clip, sourceHipsY, targetHipsY, sourceRest, targetRest) {
  const ratio = sourceHipsY > 1e-5 ? targetHipsY / sourceHipsY : 1;
  const tracks = [];
  const q = new THREE.Quaternion();
  const delta = new THREE.Quaternion();
  for (const track of clip.tracks) {
    const [node, prop] = track.name.split('.');
    if (prop === 'quaternion') {
      const t = track.clone();
      const sRest = sourceRest.get(node);
      const tRest = targetRest.get(node);
      if (sRest && tRest) {
        const sInv = sRest.clone().invert();
        for (let i = 0; i < t.values.length; i += 4) {
          q.fromArray(t.values, i);
          delta.copy(sInv).multiply(q);
          q.copy(tRest).multiply(delta);
          q.toArray(t.values, i);
        }
      }
      tracks.push(t);
    } else if (prop === 'position' && /hips/i.test(node)) {
      const t = track.clone();
      for (let i = 0; i < t.values.length; i++) t.values[i] *= ratio;
      tracks.push(t);
    }
    // every other position track, and every scale track, is dropped on purpose
  }
  return new THREE.AnimationClip(clip.name, clip.duration, tracks);
}

/**
 * A looping 4-second idle from one frame of another clip: every track holds that frame's value, except a small
 * chest rise on the spine and a tiny hip bob, so the crawler breathes instead of freezing like a statue.
 */
function makeStandClip(source, frameTime = 0) {
  const period = 4;
  const times = [0, period / 2, period];
  const breathe = new THREE.Quaternion().setFromAxisAngle(new THREE.Vector3(1, 0, 0), THREE.MathUtils.degToRad(1.6));
  const q = new THREE.Quaternion();
  const tracks = [];

  for (const track of source.tracks) {
    const size = track.getValueSize();
    const interpolant = track.createInterpolant(new Float32Array(size));
    const held = Array.from(interpolant.evaluate(frameTime));
    const [node, prop] = track.name.split('.');
    let mid = held;

    if (prop === 'quaternion' && /^Spine0?[12]?$/i.test(node)) {
      q.fromArray(held).multiply(breathe);
      mid = q.toArray();
    } else if (prop === 'position' && /hips/i.test(node)) {
      mid = [held[0], held[1] + 0.6, held[2]]; // Meshy rigs are in centimetres under a 0.01 armature
    }

    tracks.push(new track.constructor(track.name, times, [...held, ...mid, ...held]));
  }
  return new THREE.AnimationClip('Idle_Stand', period, tracks);
}

export function createViewer(canvas, { transparent = false, groundGlow = true, autoRotate = false, interactive = true } = {}) {
  const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: transparent });
  renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
  renderer.outputColorSpace = THREE.SRGBColorSpace;
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = 1.1;
  if (transparent) renderer.setClearColor(0x000000, 0);

  const scene = new THREE.Scene();
  const camera = new THREE.PerspectiveCamera(30, 1, 0.01, 100);

  scene.add(new THREE.HemisphereLight(0xcfe8ff, 0x1a1030, 1.4));
  const key = new THREE.DirectionalLight(0xffffff, 2.2);
  key.position.set(2, 4, 3);
  scene.add(key);
  const rim = new THREE.DirectionalLight(0x48d8ff, 2.0);
  rim.position.set(-3, 2, -3);
  scene.add(rim);

  // The System's floor disc: the same cyan as the in-game windows.
  let ring = null;
  if (groundGlow) {
    const tex = (() => {
      const c = document.createElement('canvas');
      c.width = c.height = 256;
      const g = c.getContext('2d');
      const grad = g.createRadialGradient(128, 128, 20, 128, 128, 128);
      grad.addColorStop(0, 'rgba(72,216,255,0.55)');
      grad.addColorStop(0.55, 'rgba(72,216,255,0.18)');
      grad.addColorStop(0.8, 'rgba(72,216,255,0.45)');
      grad.addColorStop(0.84, 'rgba(72,216,255,0.0)');
      g.fillStyle = grad;
      g.fillRect(0, 0, 256, 256);
      return new THREE.CanvasTexture(c);
    })();
    ring = new THREE.Mesh(
      new THREE.PlaneGeometry(1, 1),
      new THREE.MeshBasicMaterial({ map: tex, transparent: true, depthWrite: false })
    );
    ring.rotation.x = -Math.PI / 2;
    ring.renderOrder = -1; // under the feet, always
    scene.add(ring);
  }

  const holder = new THREE.Group();
  scene.add(holder);

  let mixer = null;
  let model = null;
  let clips = new Map();
  let current = null;
  let idleAction = null;
  let actionsById = new Map();
  let disposed = false;
  let yaw = 0;
  let dragging = false;
  let lastX = 0;
  const clock = new THREE.Clock();

  function resize() {
    const w = canvas.clientWidth || canvas.width;
    const h = canvas.clientHeight || canvas.height;
    renderer.setSize(w, h, false);
    camera.aspect = w / Math.max(1, h);
    camera.updateProjectionMatrix();
  }
  new ResizeObserver(resize).observe(canvas);
  resize();

  canvas.addEventListener('pointerdown', (e) => {
    if (!interactive || e.button !== 0) return; // middle button belongs to the action wheel
    dragging = true;
    lastX = e.clientX;
  });
  window.addEventListener('pointerup', () => (dragging = false));
  window.addEventListener('pointermove', (e) => {
    if (!dragging) return;
    yaw += (e.clientX - lastX) * 0.01;
    lastX = e.clientX;
  });

  function frame(obj) {
    const box = new THREE.Box3().setFromObject(obj);
    const size = box.getSize(new THREE.Vector3());
    const centre = box.getCenter(new THREE.Vector3());
    obj.position.x -= centre.x;
    obj.position.z -= centre.z;
    obj.position.y -= box.min.y;
    const height = size.y || 1.7;
    // Framed with headroom: several clips (the spin jump, the climb) leave the ground by most of a body height.
    const dist = (height * 0.95) / Math.tan(THREE.MathUtils.degToRad(camera.fov / 2));
    camera.position.set(0, height * 0.75, dist);
    camera.lookAt(0, height * 0.7, 0);
    if (ring) ring.scale.setScalar(Math.max(size.x, size.z, height * 0.6) * 1.6);
    return height;
  }

  function placeholder() {
    const g = new THREE.Group();
    const mat = new THREE.MeshStandardMaterial({ color: 0x2a3550, emissive: 0x0c2a40, roughness: 0.6 });
    const body = new THREE.Mesh(new THREE.CapsuleGeometry(0.28, 1.0, 6, 16), mat);
    body.position.y = 0.78;
    const head = new THREE.Mesh(new THREE.SphereGeometry(0.2, 24, 16), mat);
    head.position.y = 1.6;
    g.add(body, head);
    return g;
  }

  async function load(modelUrl, libraryUrls = []) {
    holder.clear();
    clips = new Map();
    actionsById = new Map();
    current = null;
    mixer = null;

    if (!modelUrl) {
      model = placeholder();
      holder.add(model);
      frame(model);
      return { hasModel: false, clips: [] };
    }

    const gltf = await loadGltf(modelUrl);
    model = gltf.scene; // the cache holds one parsed copy per file; the sheet shows one crawler at a time
    model.traverse((o) => {
      if (o.isMesh) {
        o.frustumCulled = false; // skinned bounds lie; a culled arm mid-dance is worse than the cost
        // Meshy exports every material as alpha-blended. The textures are opaque, and blending makes the
        // body sort badly against itself and the floor (see-through feet), so treat them as opaque.
        for (const m of [].concat(o.material)) {
          m.transparent = false;
          m.depthWrite = true;
        }
      }
    });
    holder.add(model);
    frame(model);

    mixer = new THREE.AnimationMixer(model);
    for (const clip of gltf.animations) {
      if (!/clip0|baselayer/i.test(clip.name)) clips.set(clip.name, clip);
    }

    const targetHipsY = gltf.userData.hipsY;
    const targetRest = gltf.userData.rest;

    // Borrow every other crawler's clips. Same bone names, different bodies - see the note at the top.
    const borrowed = await Promise.allSettled(libraryUrls.filter((u) => u !== modelUrl).map(loadGltf));
    for (const result of borrowed) {
      if (result.status !== 'fulfilled') continue;
      const src = result.value;

      for (const clip of src.animations) {
        if (clips.has(clip.name) || /clip0|baselayer/i.test(clip.name)) continue;
        clips.set(clip.name, retargetClip(clip, src.userData.hipsY, targetHipsY, src.userData.rest, targetRest));
      }
    }

    // There is no idle clip anywhere in the Meshy set ("Alert" leans forward and looks around), so the
    // standing idle is built: the first frame of Agree_Gesture, which is a relaxed neutral stance, held and
    // given a slow breath. Agree_Gesture is in the library, so every crawler gets it, retargeted if needed.
    const standSource = clips.get('Agree_Gesture');
    if (standSource) clips.set('Idle_Stand', makeStandClip(standSource));

    mixer.addEventListener('finished', () => {
      if (idleAction) crossTo(idleAction);
    });

    return { hasModel: true, clips: [...clips.keys()] };
  }

  function crossTo(action, fade = 0.3) {
    if (!action || action === current) return;
    action.reset();
    action.setEffectiveWeight(1);
    action.play();
    if (current) current.crossFadeTo(action, fade, false);
    current = action;
  }

  function actionFor(clipName, loop) {
    const clip = clips.get(clipName);
    if (!clip || !mixer) return null;
    const a = mixer.clipAction(clip);
    a.setLoop(loop ? THREE.LoopRepeat : THREE.LoopOnce, Infinity);
    a.clampWhenFinished = !loop;
    return a;
  }

  /** Register the roster's action list (id -> clip). Returns the ones this model can actually play. */
  function setActions(list) {
    actionsById = new Map();
    const available = [];
    for (const def of list) {
      const a = actionFor(def.clip, def.loop);
      if (a) {
        actionsById.set(def.id, a);
        available.push(def);
      }
    }
    idleAction = actionsById.get('idle') || actionsById.get('stance') || actionsById.get('walk') || null;
    if (idleAction) crossTo(idleAction, 0);
    return available;
  }

  function play(id) {
    const a = actionsById.get(id);
    if (a) crossTo(a);
    return Boolean(a);
  }

  function isPlaying(id) {
    return current && current === actionsById.get(id);
  }

  /** Buddy mode: turn to face left (-1), right (+1) or the viewer (0). */
  function face(dir) {
    yaw = dir === 0 ? 0 : dir > 0 ? Math.PI / 2 : -Math.PI / 2;
  }

  function loop() {
    if (disposed) return;
    requestAnimationFrame(loop);
    const dt = Math.min(clock.getDelta(), 0.1);
    if (mixer) mixer.update(dt);
    if (autoRotate && !dragging) yaw += dt * 0.15;
    holder.rotation.y += (yaw - holder.rotation.y) * Math.min(1, dt * 8);
    renderer.render(scene, camera);
  }
  loop();

  /** Debug/self-test: hold one clip at one moment. */
  function scrub(clipName, t) {
    const a = actionFor(clipName, true);
    if (!a || !mixer) return false;
    mixer.stopAllAction();
    current = null;
    a.reset().play();
    a.paused = true;
    a.time = t;
    mixer.update(0);
    return true;
  }

  return {
    load,
    scrub,
    clipNames: () => [...clips.keys()],
    setActions,
    play,
    isPlaying,
    face,
    resize,
    dispose() {
      disposed = true;
      renderer.dispose();
    },
  };
}
