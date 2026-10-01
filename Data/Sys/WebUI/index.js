
const notyf = new Notyf();

const fullscreenButton = document.getElementById('fullscreen');

let dataChannel;

fullscreenButton.onclick = async () => {
  if (document.fullscreenElement) return;
  await document.documentElement.requestFullscreen();
  screen.orientation.lock('landscape');
};

document.addEventListener('fullscreenchange', () => {
  fullscreenButton.hidden = !!document.fullscreenElement;
});

function startBitrateLogging(pc, intervalMs = 1000) {
  let state = {};

  setInterval(async () => {
    const pc_stats = await pc.getStats();
    const now = performance.now();

    pc_stats.forEach(report => {
      if (report.type !== 'inbound-rtp') return;

      const key = `${report.kind}-${report.id}`;

      if (key in state) {
        let prev_stats = state[key];
        const bytes = report.bytesReceived - prev_stats.bytes_received;
        const seconds = (now - prev_stats.time) / 1000;
        const bitrate = bytes * 8 / seconds;

        console.log(`${key} bitrate: ${(bitrate / 1000).toFixed(2)} kbps`);
      } else {
        state[key] = {};
      }

      state[key].bytes_received = report.bytesReceived;
      state[key].time = now;
    });
  }, intervalMs);
}

function create_peer_connection(ws, iceServers) {
  iceServers = iceServers.map(urls => ({urls}));
  console.log('iceServers: ', iceServers);

  const pc = new RTCPeerConnection({iceServers});

  pc.addTransceiver('video', {direction: 'recvonly'});
  pc.addTransceiver('audio', {direction: 'recvonly'});

  pc.ondatachannel = e => {
    if (e.channel.label !== 'control') return;

    dataChannel = e.channel;

    dataChannel.onopen = () => {
      notyf.success('Data channel opened');
    };

    dataChannel.onclose = () => {
      notyf.success('Data channel closed');
    };

    dataChannel.onmessage = e => {
      console.log('dataChannel.onmessage:', e.data);
    };
  };

  pc.ontrack = e => {
    console.log('ontrack:', e.track.kind);

    jitter_buffer_target_map = {
      'video': 0,
      'audio': 20,
    };
    e.receiver.jitterBufferTarget = jitter_buffer_target_map[e.track.kind];

    video.srcObject.addTrack(e.track);
  };

  pc.onconnectionstatechange = () => {
    notyf.success('WebRTC: ' + pc.connectionState);
  };

  pc.onicecandidate = e => {
    if (e.candidate && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({
        type: 'candidate',
        candidate: e.candidate.candidate,
        mid: e.candidate.sdpMid
      }));
    }
  };

  startBitrateLogging(pc);

  return pc;
}

function load_gba(index) {
  notyf.success(`Connecting to GBA${index + 1}`);

  document.title = `Dolphin Web Interface - GBA${index + 1}`;

  for (let i = 0; i != 4; ++i) {
    document.getElementById(`gba${i + 1}`).hidden = true;
  }
  fullscreenButton.hidden = false;

  const video = document.getElementById('video');
  video.srcObject = new MediaStream();
  video.play();

  const ws = new WebSocket(`ws://${location.host}/gba${index + 1}`);

  ws.onopen = () => {
    notyf.success('Signaling connected');
    ws.send(JSON.stringify({type: 'request'}));
  };

  let pc;

  ws.onmessage = async e => {
    const m = JSON.parse(e.data);

    if (m.type === 'offer') {
      iceServers = m.iceServers ? m.iceServers.split(' ') : [];
      pc = create_peer_connection(ws, iceServers);

      await pc.setRemoteDescription({type: 'offer', sdp: m.description});

      const answer = await pc.createAnswer();
      await pc.setLocalDescription(answer);

      ws.send(JSON.stringify({type: 'answer', description: answer.sdp}));
    } else if (m.type === 'candidate') {
      try {
        await pc.addIceCandidate({candidate: m.candidate, sdpMid: m.mid});
      } catch (err) {
        console.warn('ICE candidate:', err);
      }
    }
  };

  ws.onerror = () => notyf.error('Signaling error');
  ws.onclose = () => notyf.success('Signaling closed');
}

for (let i = 0; i != 4; ++i) {
  document.getElementById(`gba${i + 1}`).onclick = async () => {
    load_gba(i);
  };
}

const {
  TargetZone,
  VirtualDPad,
  VirtualButton,
} = window.OmniPad;

const controls = document.getElementById('controls');

// OmniPad overlay
controls.style.position = 'absolute';
controls.style.inset = '0';
controls.style.pointerEvents = 'none';

// Target the video
const stage = new TargetZone(controls, {
  widgetId: '$video',
  cursorEnabled: false,
  layout: {
    stickySelector: '#video',
  },
});

const keyMap = {
  ArrowUp: 'up',
  ArrowDown: 'down',
  ArrowLeft: 'left',
  ArrowRight: 'right',

  KeyX: 'a',
  KeyZ: 'b',
  Enter: 'start',
  ShiftLeft: 'select',
  KeyA: 'l',
  KeyS: 'r',
};

// D-pad
new VirtualDPad(controls, {
  targetStageId: '$video',

  mapping: {
    up: 'ArrowUp',
    down: 'ArrowDown',
    left: 'ArrowLeft',
    right: 'ArrowRight',
  },

  layout: {
    left: '5%',
    bottom: '10%',
    width: '140px',
    height: '140px',
    zIndex: 100,
  },
});

// A button
new VirtualButton(controls, {
  label: 'A',
  targetStageId: '$video',
  mapping: {code: 'KeyX'},

  layout: {
    right: '5%',
    bottom: '25%',
    width: '70px',
    height: '70px',
    zIndex: 100,
  },
});

// B button
new VirtualButton(controls, {
  label: 'B',
  targetStageId: '$video',
  mapping: {code: 'KeyZ'},

  layout: {
    right: '15%',
    bottom: '5%',
    width: '70px',
    height: '70px',
    zIndex: 100,
  },
});

// L button
new VirtualButton(controls, {
  label: 'L',
  targetStageId: '$video',
  mapping: {code: 'KeyA'},

  layout: {
    left: '5%',
    top: '5%',
    width: '70px',
    height: '70px',
    zIndex: 100,
  },
});

// R button
new VirtualButton(controls, {
  label: 'R',
  targetStageId: '$video',
  mapping: {code: 'KeyS'},

  layout: {
    right: '5%',
    top: '5%',
    width: '70px',
    height: '70px',
    zIndex: 100,
  },
});

// Select
new VirtualButton(controls, {
  label: 'SELECT',
  targetStageId: '$video',
  mapping: {code: 'ShiftLeft'},

  layout: {
    right: '55%',
    bottom: '5%',
    width: '65px',
    height: '40px',
    zIndex: 100,
  },
});

// Start
new VirtualButton(controls, {
  label: 'START',
  targetStageId: '$video',
  mapping: {code: 'Enter'},

  layout: {
    left: '55%',
    bottom: '5%',
    width: '65px',
    height: '40px',
    zIndex: 100,
  },
});

const button_bits = {
  a: 0x0001,
  b: 0x0002,
  select: 0x0004,
  start: 0x0008,
  right: 0x0010,
  left: 0x0020,
  up: 0x0040,
  down: 0x0080,
  r: 0x0100,
  l: 0x0200,
};

let gba_keys = 0;

function updateKeys(new_gba_keys) {
  if (new_gba_keys == gba_keys) return;

  gba_keys = new_gba_keys;

  dataChannel.send(JSON.stringify({
    'keys': gba_keys,
  }));
}

function handleKey(event, pressed) {
  const name = keyMap[event.code];
  if (!name) return;

  const value = button_bits[name];

  let new_gba_keys = gba_keys;
  if (pressed)
    new_gba_keys |= value;
  else
    new_gba_keys &= ~value;

  updateKeys(new_gba_keys);
}

const gamepadMap = {
  'up': [12],
  'down': [13],
  'left': [14],
  'right': [15],
  'a': [1, 3],
  'b': [0, 2],
  'l': [4, 6],
  'r': [5, 7],
  'select': [8],
  'start': [9],
};

window.addEventListener('keydown', e => {
  handleKey(e, true);

  // Hide on screen controls on real button press.
  if (e.isTrusted) controls.hidden = true;
});
window.addEventListener('keyup', e => handleKey(e, false));

video.addEventListener('click', () => {
  controls.hidden = false;
});

function poll() {
  const gamepad = navigator.getGamepads()[0];

  if (gamepad) {
    let gamepad_gba_keys = 0;

    Object.entries(gamepadMap).forEach(([gba_btn_name, mappings]) => {
      const pressed =
          mappings.map(i => gamepad.buttons[i]).some(b => b.pressed);
      if (pressed) gamepad_gba_keys |= button_bits[gba_btn_name];
    });

    updateKeys(gamepad_gba_keys);

    // Hide on screen controls on real button press.
    if (gamepad_gba_keys != 0) controls.hidden = true;
  }

  requestAnimationFrame(poll);
}

poll();
