# OptForAudio
Utility to temporarily change Windows system settings for improved performance during real time audio generation.

I wrote this to do repetitive system housekeeping necessary to run Amplitube on a Dell laptop with significantly reduced audio dropouts and buffer underruns.

It's a command line/console program that:
- Runs elevated
- Disables display power down
- Disables screensaver timeout
- Disables Wi-Fi interface
- Disables CPU throttling
- Launches a list of programs (but not elevated)
- Waits for all of the programs to exit
- Restores all changes it made
