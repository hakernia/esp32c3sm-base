These are base projects for client and server built on noname ESP32C3 Super Micro boards.

To build properly create your own secrets_local.h, based on secrets_example.h included in the repo.
You may want to add it to each subprojects' directory or to common library like SharedSecrets etc.

Client
The client is in active state by default.
It sends simple messages based on changing states of GPIO 1-4
It enters sleep mode on Server's request.
Exit from sleep mode by changing GPIO 1-4 or by timer (commented out).

Server
Stays in Active state.
Receives messages from client and drops info to Serial Monitor.
Getting Client asleep is not used now but can be implemented with required logic.
