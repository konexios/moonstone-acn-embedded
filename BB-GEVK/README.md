# README #

### How to use ###
1. Install the software (IOT Development Kit IDE etc) concern EVBUM2497/D IoT Development Kit (IDK) Quick Start Guide (SW INSTALLATION STEPS path) 
Assume that the On Semi IDK placed into /some/On Semiconductor folder

2. Clone the github repository into /some/path
cd /some/path
git clone git@github.com:arrow-acs/acn-embedded.git --depth 1 --recursive

3. Copy the *.a files from /some/path/acn-embedded/BB-GEVK/libs/ to /some/OnSemiconductor/IDK/lib

4. Launch the IDE
choose the /some/path/acn-embedded as a Workspace (into Select a workspace window)

5. In a Project Explorer window click right mouse button and select "Import..."
General->Existing Projects into Workspace
Select root directory: /some/path/acn-embedded/BB-GEVK/
Press "Finish"

6. Set up the config.cfg file if you need 
( Board COMM Port = COM1 or another )

7. You already have a binary file for a flashing the board but you can to rebuild it
In this case replace
TL_PATH = C:\GNUToolchain\bin
by the actual path of the toolchain

8. Program the board by USB Programming button
