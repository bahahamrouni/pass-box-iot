# PASS-BOX: pass-through chamber supervision (ESP32, MQTT, Node-RED)

Supervision of a pass-box between a contaminated area and a sterile area: two interlocked doors, a decontamination cycle, an emergency stop, local status on an LCD, and telemetry to a Node-RED dashboard.

Course project (IoT, ENICarthage, December 2025) by  Bahaeddine Hamrouni. Simulation only: it runs in the Wokwi simulator, not on physical hardware.

## What it does

- **Interlock:** the two doors cannot be open at the same time.
- **Cycle:** an 8-step decontamination cycle runs as a non-blocking state machine (`millis()`, no blocking delays).
- **Emergency stop:** a button interrupts the cycle.
- **Local status:** a 16x2 I2C LCD shows the state and error messages.
- **Telemetry:** states are published over MQTT (`porte/contaminee`, `porte/sterile`, `urgence`, `cycle/depart`, `cycle/etape`).
- **Dashboard:** a Node-RED flow shows the states, keeps a history table, logs to CSV and can send an e-mail report.

## Run

1. Open the Wokwi project: https://wokwi.com/projects/448681192686986241 and paste `firmware/pass-box.cpp` if the project is not already loaded.
2. Start it. The simulated board joins the `Wokwi-GUEST` network and connects to the MQTT broker.
3. In Node-RED, import `node-red/flows.json`. Configure the e-mail node with your own SMTP sender before using it (the sender address in the file is a placeholder).

## Known limitations

- **Remote messages bypass the interlock.** The MQTT callback sets door states from incoming messages without applying the interlock logic, so anyone publishing on those topics could open both doors. A real system must apply the interlock to every command.
- **Public, unauthenticated broker.** It uses `broker.hivemq.com` on port 1883 without TLS or credentials and generic topic names, so anyone can read and publish to them. A real deployment needs a private broker with TLS, authentication and topic ACLs, and separate status and command topics.
- The firmware republishes inside the callback it is subscribed to, guarded only by a last-value check.
- Simulation only, and the button inputs have no debouncing.

## Layout

```
firmware/pass-box.cpp   ESP32 firmware (Arduino framework)
node-red/flows.json     dashboard, history, CSV and e-mail flow
```
