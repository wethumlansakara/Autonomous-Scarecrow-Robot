import { useEffect, useMemo, useState } from "react";
import { onValue, ref, set } from "firebase/database";
import { db } from "./firebase";
import "./App.css";

const KEY_TO_COMMAND = {
  w: "F",
  a: "L",
  s: "B",
  d: "R",
  " ": "S",
};

const clamp = (value, min, max) => Math.min(max, Math.max(min, value));

const normalizeBit = (value, fallback = 0) => {
  if (typeof value === "boolean") {
    return value ? 1 : 0;
  }

  const numeric = Number(value);
  if (Number.isFinite(numeric)) {
    return numeric !== 0 ? 1 : 0;
  }

  return fallback;
};

const normalizeMode = (value) => {
  if (typeof value !== "string" || value.length === 0) {
    return "A";
  }

  const mode = value.charAt(0).toUpperCase();
  return mode === "M" ? "M" : "A";
};

const normalizeCommand = (value) => {
  if (typeof value !== "string" || value.length === 0) {
    return "S";
  }

  const command = value.charAt(0).toUpperCase();
  return ["F", "B", "L", "R", "S"].includes(command) ? command : "S";
};

const DEFAULT_ROBOT_STATE = {
  command: "S",
  mode: "A",
  servo: 1,
  buzzer: 0,
  speed: 90,
  ultrasonic: 0,
  pir: 0,
  actuator: 0,
};

function App() {
  const [robot, setRobot] = useState(DEFAULT_ROBOT_STATE);
  const [connection, setConnection] = useState("Disconnected");
  const [activity, setActivity] = useState("Waiting for data...");

  const robotRef = useMemo(() => ref(db, "robot"), []);
  const connectedRef = useMemo(() => ref(db, ".info/connected"), []);

  const writeField = async (field, value, label) => {
    await set(ref(db, `robot/${field}`), value);
    setActivity(`Updated ${label}: ${value}`);
  };

  const sendCommand = async (command) => {
    await writeField("command", command, "command");
  };

  useEffect(() => {
    const unsubscribeRobot = onValue(robotRef, (snapshot) => {
      const value = snapshot.val() ?? {};

      setRobot({
        command: normalizeCommand(value.command),
        mode: normalizeMode(value.mode),
        servo: normalizeBit(value.servo, 1),
        buzzer: normalizeBit(value.buzzer, 0),
        speed: clamp(Number(value.speed) || 0, 0, 255),
        ultrasonic: normalizeBit(value.ultrasonic, 0),
        pir: normalizeBit(value.pir, 0),
        actuator: normalizeBit(value.actuator, 0),
      });
    });

    const unsubscribeConnected = onValue(connectedRef, (snapshot) => {
      setConnection(snapshot.val() === true ? "Connected" : "Disconnected");
    });

    return () => {
      unsubscribeRobot();
      unsubscribeConnected();
    };
  }, [connectedRef, robotRef]);

  useEffect(() => {
    const onKeyDown = (event) => {
      const key = event.key.toLowerCase();
      const mappedCommand = KEY_TO_COMMAND[key];
      if (!mappedCommand) {
        return;
      }

      if (key === " ") {
        event.preventDefault();
      }

      sendCommand(mappedCommand);
    };

    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, []);

  return (
    <main className="app">
      <h1>Scarecrow Robot Dashboard</h1>
      <p className="subtitle">Realtime control panel synced with ESP32 + Firebase</p>

      <section className="panel">
        <h2>Movement</h2>
        <div className="grid" aria-label="Robot movement controls">
          <div />
          <button type="button" onClick={() => sendCommand("F")}>↑</button>
          <div />

          <button type="button" onClick={() => sendCommand("L")}>←</button>
          <button type="button" className="stop" onClick={() => sendCommand("S")}>STOP</button>
          <button type="button" onClick={() => sendCommand("R")}>→</button>

          <div />
          <button type="button" onClick={() => sendCommand("B")}>↓</button>
          <div />
        </div>
      </section>

      <section className="panel panel-inline" aria-label="Robot mode control">
        <h2>Mode</h2>
        <div className="segmented">
          <button
            type="button"
            className={robot.mode === "A" ? "active" : ""}
            onClick={() => writeField("mode", "A", "mode")}
          >
            Auto
          </button>
          <button
            type="button"
            className={robot.mode === "M" ? "active" : ""}
            onClick={() => writeField("mode", "M", "mode")}
          >
            Manual
          </button>
        </div>
      </section>

      <section className="panel panel-inline" aria-label="Servo and buzzer control">
        <h2>Switches</h2>
        <div className="segmented">
          <button
            type="button"
            className={robot.servo === 1 ? "active" : ""}
            onClick={() => writeField("servo", robot.servo === 1 ? 0 : 1, "servo")}
          >
            Servo: {robot.servo === 1 ? "ON" : "OFF"}
          </button>
          <button
            type="button"
            className={robot.buzzer === 1 ? "active" : ""}
            onClick={() => writeField("buzzer", robot.buzzer === 1 ? 0 : 1, "buzzer")}
          >
            Buzzer: {robot.buzzer === 1 ? "ON" : "OFF"}
          </button>
        </div>
      </section>

      <section className="panel slider" aria-label="Speed control">
        <h2>Speed</h2>
        <input
          type="range"
          min="0"
          max="255"
          value={robot.speed}
          onChange={(event) => {
            const nextSpeed = clamp(Number(event.target.value), 0, 255);
            writeField("speed", nextSpeed, "speed");
          }}
        />
        <p className="value">{robot.speed}</p>
      </section>

      <section className="panel indicators" aria-label="Sensor and actuator state">
        <h2>Live States</h2>
        <div className="chips">
          <p className={robot.ultrasonic === 1 ? "chip danger" : "chip"}>
            Ultrasonic: {robot.ultrasonic === 1 ? "Obstacle" : "Clear"}
          </p>
          <p className={robot.pir === 1 ? "chip danger" : "chip"}>
            PIR: {robot.pir === 1 ? "Motion" : "No Motion"}
          </p>
          <p className={robot.actuator === 1 ? "chip active" : "chip"}>
            Actuator: {robot.actuator === 1 ? "ON" : "OFF"}
          </p>
        </div>
      </section>

      <section className="panel summary" aria-label="Current robot summary">
        <p>Connection: {connection}</p>
        <p>Command: {robot.command}</p>
        <p>Mode: {robot.mode}</p>
        <p>{activity}</p>
      </section>
    </main>
  );
}

export default App;
