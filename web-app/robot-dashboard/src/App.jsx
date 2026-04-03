import { useEffect, useMemo, useState } from "react";
import { onValue, ref, set } from "firebase/database";
import { db } from "./firebase";
import "./App.css";

const COMMANDS = {
  w: "F",
  a: "L",
  s: "B",
  d: "R",
  " ": "S",
};

function App() {
  const [command, setCommand] = useState("-");
  const [speed, setSpeed] = useState(200);
  const [status, setStatus] = useState("Disconnected");

  const robotCommandRef = useMemo(() => ref(db, "robot/command"), []);
  const robotSpeedRef = useMemo(() => ref(db, "robot/speed"), []);
  const connectedRef = useMemo(() => ref(db, ".info/connected"), []);

  const sendCommand = async (cmd) => {
    await set(robotCommandRef, cmd);
    setStatus(`Sent: ${cmd}`);
  };

  const onSpeedChange = async (nextSpeed) => {
    await set(robotSpeedRef, Number(nextSpeed));
  };

  useEffect(() => {
    const unsubscribeCommand = onValue(robotCommandRef, (snapshot) => {
      const value = snapshot.val();
      if (typeof value === "string" && value.length > 0) {
        setCommand(value);
      }
    });

    const unsubscribeSpeed = onValue(robotSpeedRef, (snapshot) => {
      const value = snapshot.val();
      if (typeof value === "number") {
        setSpeed(value);
      }
    });

    const unsubscribeConnected = onValue(connectedRef, (snapshot) => {
      setStatus(snapshot.val() === true ? "Connected" : "Disconnected");
    });

    return () => {
      unsubscribeCommand();
      unsubscribeSpeed();
      unsubscribeConnected();
    };
  }, [connectedRef, robotCommandRef, robotSpeedRef]);

  useEffect(() => {
    const onKeyDown = (event) => {
      const key = event.key.toLowerCase();
      const mappedCommand = COMMANDS[key];
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
      <h1>Robot Controller</h1>

      <section className="grid" aria-label="Robot movement controls">
        <div />
        <button type="button" onClick={() => sendCommand("F")}>↑</button>
        <div />

        <button type="button" onClick={() => sendCommand("L")}>←</button>
        <button type="button" className="stop" onClick={() => sendCommand("S")}>STOP</button>
        <button type="button" onClick={() => sendCommand("R")}>→</button>

        <div />
        <button type="button" onClick={() => sendCommand("B")}>↓</button>
        <div />
      </section>

      <section className="slider" aria-label="Speed control">
        <p>Speed</p>
        <input
          type="range"
          min="0"
          max="255"
          value={speed}
          onChange={(event) => onSpeedChange(event.target.value)}
        />
      </section>

      <p className="status">Command: {command} | Speed: {speed}</p>
      <p className="status">{status}</p>
    </main>
  );
}

export default App;
