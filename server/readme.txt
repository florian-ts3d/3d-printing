To build:

Set POLYGONICA_DIR environment variable to Polgyonica install dir.
Set EXCHANGE_INSTALL_DIR environment variable to Exchnage install dir.

Download hoops_license.h and add to exchange/include dir

Build.

You can run PgServer by itself or set it up with as a restartable service on Windows Server using NSSM:

- Download https://nssm.cc
- At a command prompt type: nssm install "polygonica_service".
- Specify PgServer.exe in the UI
- In windows services start the service, make sure it is set to autorestart.