M2 GI 2025-2026\
IOT, Partie LPWAN\
Binôme: LAGRANGE Victoria, YANG Déborah

# Simple test of driver

## Build

```bash
make
make BOARD=wyres-base DRIVER=sx1272 -j 16
make -j 4 flash
```

## Connection

```bash
tio
tio -b 115200 -m INLCRNL /dev/ttyUSB0
```

## Organization

* _main.c_: contains the main function and the shell commands
* _message.h_: contains the definition of the **chat_message_t** struct
* _users.h_: contains the definition of the **user_t** struct and the user management functions
* _rapport_analyse_logs.ipynb_: contains the analysis of the logs

## Usage

In term type help to see commands

type command to see how to use

basic use to check communication between 2 boards :

* On receving board :
```
> setup 125 12 5
> channel set 868000000
> listen
```

* On sending board :
```
> setup 125 12 5
> channel set 868000000
> send This\ is\ RIOT!
```

* On receiving board you should see something like:
```
123@*:0:Hello world!  [RSSI:-42 SNR:10]
```
