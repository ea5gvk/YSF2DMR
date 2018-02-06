# Description

This is the source code of YSF2DMR, a software for digital voice conversion from Yaesu System Fusion to DMR digital mode, based on Jonathan G4KLX's [MMDVM](https://github.com/g4klx) software. This software is very experimental and still under development.

You can use this software and YSFGateway at the same time, with the default YSF UDP ports (42000 and 42013). In this case, you can select the pseudo YSF2DMR reflector in the Wires-X list provided by YSFGateway.

Also, you can connect directly with MMDVMHost, changing the following ports in [YSF Network] section (YSF2DMR.ini):

    DstPort=3200
    LocalPort=4200

You have to select the destination DMR TG to connect (or private call):

    StartupDstId=730
    StartupPC=0

YSF2DMR looks for DMR ID of the YSF callsign in the DMRIds.dat file, in case of no coincidence, it will use your DMR ID. Also, all IDs from DMR Network will be converted to callsigns and you will see it at the display of your YSF radio. 

This software is licenced under the GPL v2 and is intended for amateur and educational use only. Use of this software for commercial purposes is strictly forbidden.
