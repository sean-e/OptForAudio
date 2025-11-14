# OptForAudio
Utility to temporarily change Windows system settings for improved performance during real time audio generation.

I wrote this to do repetitive system housekeeping necessary to run [IK Multimedia Amplitube](https://www.ikmultimedia.com/products/amplitube5/) on an old Dell laptop with significantly reduced audio dropouts and buffer underruns (at an ASIO buffer size of 64 samples).

It's a command line/console program that:
- Runs elevated
- Disables display power down (due more to convenience than to performance)
- Disables screensaver timeout (due more to convenience than to performance)
- Disables Wi-Fi interface
- Disables CPU throttling ([see C States](https://support.presonus.com/hc/en-us/articles/360028620552-Quantum-Disabling-C-States-on-a-Windows-10-computer))
- Disables Microsoft ACPI-Compliant Control Method Battery (device ID ACPI\PNP0C0A\1)
- Launches a list of programs (but not elevated)
- Disables CPU affinity of cores 0 and 1 of launched programs (but not the util programs) ([related to this advice for nvidia drivers](https://www.bluecataudio.com/Blog/tip-of-the-day/solving-audio-dropouts-dpc-latency-issues-with-nvidia-drivers-on-windows/))
- Waits for all of the programs to exit
- Restores all changes it made


Optional command-line params:
- `-xDisplay`		prevent change to display power down
- `-xScreensaver`	prevent change to screensaver timeout
- `-xCpuThrottle`	prevent change to CPU throttling
- `-xWifi`			prevent change to Wi-Fi interface
- `-xCoreAffinity`	prevent change to CPU core affinity of launched programs
- `-xAcpi` 			prevent change to Microsoft ACPI-Compliant Control Method Battery
- `-runUtils`		starts [LatencyMon](https://www.resplendence.com/latencymon) (if found at C:\Program Files\LatencyMon\LatMon.exe) and [Rightmark PPM Panel](https://sourceforge.net/projects/rightmark/) (if found at C:\Program Files\RightMark\ppmpanel\ppmpanel.exe)

Additional reference:
- https://www.sweetwater.com/sweetcare/articles/causes-of-dpc-latency-and-how-to-fix-them/
- https://support.presonus.com/hc/en-us/articles/360025279231-Optimizing-Your-Computer-for-Audio-Windows-10
- https://www.bluecataudio.com/Blog/tip-of-the-day/how-to-optimize-a-windows-laptop-for-low-latency-real-time-audio/
- https://www.cantabilesoftware.com/glitchfree/
