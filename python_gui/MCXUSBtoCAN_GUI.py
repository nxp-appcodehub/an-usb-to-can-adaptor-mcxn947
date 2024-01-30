#!/usr/bin/env python
# -*- coding: UTF-8 -*-
#
# Copyright 2019-2023 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

################################### Import Module ############################
import serial
import tkinter as tk
from tkinter import *
from tkinter import ttk
import datetime
import threading
import time

################################## Drop down variable options ################ 
can_baudrate = [
'100000',
'250000',
'500000',
'800000',
'1000000'
]

can_fd_baudrate = [
'1000000',
'2000000',
'4000000',
'5000000'
]

#Get current time to calculate timestamp
initTime = datetime.datetime.now()

serialComVar = None
serThread = None

################################### Get Available Ports ######################
import serial.tools.list_ports
availablePorts = []
for portList in serial.tools.list_ports.comports():
    availablePorts.append(portList.name)

def serial_reception():
    global serialComVar
    dataRx = ""
    while True:
        bytesToRead = serialComVar.inWaiting()
        dataRaw = str(serialComVar.read(bytesToRead))
        deltaRxtime = (datetime.datetime.now() - initTime)
        if(dataRaw[2] == 'F' and dataRaw[3] == 'D'):
            dataRx = ""
            if(dataRaw[4] == 's'):
                canIdRx = dataRaw[5] + dataRaw[6] + dataRaw[7]
                messageRxsize = dataRaw[8]
                if(messageRxsize == 'A'):
                    messageRxsize = "10"
                elif(messageRxsize == 'B'):
                    messageRxsize = "11"
                elif(messageRxsize == 'C'):
                    messageRxsize = "12"
                elif(messageRxsize == 'D'):
                    messageRxsize = "13"
                elif(messageRxsize == 'E'):
                    messageRxsize = "14"
                elif(messageRxsize == 'F'):
                    messageRxsize = "15"
                else:
                    messageRxsize = dataRaw[8]
                counter = 9
                while (counter <= (bytesToRead)):
                    dataRx += dataRaw[counter]
                    counter += 1

                canFrame.insert('', 0, text="1", values=(deltaRxtime,"Rx", "FD", canIdRx, messageRxsize, dataRx ))
                print(dataRaw)

        else:
            if(dataRaw[2] == 's' or dataRaw[2] == 'S'):
                dataRx = ""
                if(dataRaw[2] == 's'):
                    canIdRx = dataRaw[3] + dataRaw[4] + dataRaw[5]
                    messageRxsize = dataRaw[6]
                    counter = 7
                    intMessageSize = (int(messageRxsize)*2) + 7
                    while (counter < (intMessageSize)): 
                        dataRx += dataRaw[counter]
                        counter += 1
                else:
                    canIdRx = dataRaw[3] + dataRaw[4] + dataRaw[5] + dataRaw[6] + dataRaw[7] + dataRaw[8] +dataRaw[9]
                    messageRxsize = dataRaw[10]
                    counter = 11
                    while (dataRaw[counter] != '/'): 
                        dataRx += dataRaw[counter]

                canFrame.insert('', 0, text="1", values=(deltaRxtime,"Rx", "", canIdRx, messageRxsize, dataRx ))
                print(dataRaw)

#################################### Create Interface######################### 
# create root window
window = Tk()
 
# root window title and dimension
window.title("MCXUSBtoCAN_GUI v1.0")
# Set geometry (widthxheight)
window.geometry('1200x550')
#window.option_add('*Font', '5')

#Label to identify Port
portTxt = Label(window, text="Port", font=("Helvetica", 10))
portTxt.place(x=10, y=30)

#Dropdown for Port COM
serialPort = StringVar(window)
serialPort.set(availablePorts[0]) # set default 

port = OptionMenu(window, serialPort,*availablePorts)
port.place(x=45, y=27)

#Label to identify Baudrate
canBaudrateTxt = Label(window, text="CAN Baudrate", font=("Helvetica", 10))
canBaudrateTxt.place(x=140, y=30)

#Dropdown menu to select baudrate
baudrate = StringVar(window)
baudrate.set(can_baudrate[4]) #Set default value

baudrateDropDown = OptionMenu(window, baudrate, *can_baudrate)
baudrateDropDown.place(x=230, y=27)

#Label to identify CAN FD Baudrate
canFdBaudrateTxt = Label(window, text="CAN FD Baudrate", font=("Helvetica", 10))
canFdBaudrateTxt.place(x=330, y=30)

#Dropdown menu to select baudrate
fdBaudrate = StringVar(window)
fdBaudrate.set(can_fd_baudrate[1]) #Set default value

fdBaudrateDropDown = OptionMenu(window, fdBaudrate, *can_fd_baudrate)
fdBaudrateDropDown.place(x=440, y=27)

#Create Connect Button

def connect_to_serial():
    global serialComVar
    global sendButton
    global serThread
    if(connectButton['text'] == 'Connect'):
        selectedPort = serialPort.get() 
        serialComVar = serial.Serial(selectedPort)
        print(selectedPort)
        connectButton['text']='Disconnect'
        connectButton['background']="dark grey"
        port.config(bg="dark grey", state="disable")
        baudrateDropDown.config(bg="dark grey", state="disable")
        fdBaudrateDropDown.config(bg="dark grey", state="disable")
        sendButton.config(bg="gray95", state="normal")  

        #create Thread to listen to serial port
        serThread = threading.Thread(target=serial_reception)
        serThread.start()

        print(baudrate.get())
        print(fdBaudrate.get())
        CanConfig = "I" + "AR" + baudrate.get()+ "FD" + fdBaudrate.get()
        print(CanConfig)
        serialComVar.write(CanConfig.encode('ASCII'))
    
    else:
        # Disable Thread if serial por is disable
        serialComVar.cancel_read()
        serialComVar.close()
        connectButton['text']='Connect'
        connectButton['background']="gray95"
        port.config(bg="gray95", state="normal")
        baudrateDropDown.config(bg="gray95", state="normal")
        fdBaudrateDropDown.config(bg="gray95", state="normal")
        sendButton.config(bg="dark grey", state="disable")
        serThread.join()
        

connectButton = Button(window, text = 'Connect', bg='gray95', bd=2, width=10, command=connect_to_serial)
connectButton.place(x=1100, y=27)

# Create Tree widget to show CAN frames
treeStyle = ttk.Style(window)
treeStyle.theme_use("alt")
#treeStyle.configure('Treeview.Heading', background='orange2', foreground='black')
canFrame = ttk.Treeview(window, column=(1,2,3,4,5,6), show='headings', height=16)
canFrame.place(x=15, y=80)

canFrame.column(1,  anchor="nw", width=100)
canFrame.heading(1, text='Time(ms)')
canFrame.column(2,  anchor="nw", width=50)
canFrame.heading(2, text='Tx/Rx')
canFrame.column(3,  anchor="nw", width=50)
canFrame.heading(3, text='Type')
canFrame.column(4,  anchor="nw", width=70)
canFrame.heading(4, text='ID')
canFrame.column(5,  anchor="nw", width=50)
canFrame.heading(5, text='DLC')
canFrame.column(6,  anchor="nw", width=850)
canFrame.heading(6, text='Data')


#Label to identify CAN Transmition Section 
canIdTxt = Label(window, text="CAN Tx Information Section", font=("Helvetica", 10))
canIdTxt.place(x=10, y=430)

#Label to identify CAN ID
canIdTxt = Label(window, text="CAN ID", font=("Helvetica", 10))
canIdTxt.place(x=10, y=460)

#Entry field for CAN ID
canId = Entry(window, bd=2, width=12)
canId.place(x=10, y=485)
canId.config(state=NORMAL)
canId.insert(0, "123")

#Label to identify CAN DLC
canDlcTxt = Label(window, text="DLC", font=("Helvetica", 10))
canDlcTxt.place(x=100, y=460)

#Entry field for CAN DLC
canDlc = Label(window, bd=2, text="8", font=("Helvetica", 10))
canDlc.place(x=102, y=485)

#Label to identify CAN Data
canDataTxt = Label(window, text="Data", font=("Helvetica", 10))
canDataTxt.place(x=150, y=460)

#Entry field for CAN Data
canData = tk.Text(window, bd=2, width=128, height=1)
canData.grid(column=128, row=1)
canData.place(x=150, y=483)
canData.insert('1.0', "1122334455667788")

#Check box for FD activation
checkboxFdVal = False

#Checkbox event#
def check_FD_Clicked():
    global checkboxFdVal
    if(checkboxFdVal == False):
        checkboxFdVal = True
    else:
        checkboxFdVal = False
        canDataMessage = canData.get("1.0", tk.END)
        messageSize = (len(canDataMessage) -1)
        if(messageSize > 16):
            canData.delete("1.16",tk.END)
            canDlc.config(text='8')

canFd = Checkbutton(window, bd=2, text="FD", command=check_FD_Clicked)
canFd.place(x=1115, y=450)

#Create Send Button
messageSize = '8'

def send_message():
    global checkboxFdVal
    global initTime
    global serialComVar
    if(checkboxFdVal == True):
        canFdActive = "FD"
    else:
        canFdActive = ""

    deltaTime = (datetime.datetime.now() - initTime)

    canFrame.insert('', 0, text="1", values=(deltaTime,"Tx", canFdActive, canId.get(), messageSize, canData.get("1.0", tk.END) ))
    
    dlcText = canDlc.cget("text")

    if(dlcText == "10"):
        dlcText = 'A'
    elif(dlcText == "11"):
        dlcText = 'B'
    elif(dlcText == "12"):
        dlcText = 'C'
    elif(dlcText == "13"):
        dlcText = 'D'
    elif(dlcText == "14"):
        dlcText = 'E'
    elif(dlcText == "15"):
        dlcText = 'F'
    else:
        dlcText = canDlc.cget("text")   
       
    mergeCanMessage = canFdActive + 's'+ canId.get() + dlcText + canData.get("1.0", tk.END)
    toSendMessage = mergeCanMessage.encode('ASCII')
    serialComVar.write(toSendMessage)
    print(toSendMessage)

sendButton = Button(window, text = 'Send', bg='gray95',  bd=2, width=10, command=send_message)
sendButton.place(x=1075, y=510)
sendButton.config(bg="dark grey", state="disable")

########################### Widget Actions ##############################

#Message Data update#
def update_can_message(event):
    global checkboxFdVal
    global messageSize
    canDataMessage = canData.get("1.0", tk.END)
    messageSize = (len(canDataMessage) -1)

    if((checkboxFdVal == False) & (messageSize > 16)):
        canData.delete("1.16",tk.END)
        messageSize = 16

    if(messageSize % 2 == 0):
        if(messageSize <= 16 ):
            messageSize = int(messageSize/2)
            canDlc.config(text=str(messageSize))
        elif(messageSize == 32):
            messageSize = 10
            canDlc.config(text=str(messageSize))
        elif(messageSize == 64):
            messageSize = 13
            canDlc.config(text=str(messageSize))
        elif(messageSize == 128):
            messageSize = 15
            canDlc.config(text=str(messageSize))
        else:
            canDlc.config(text='Err')
    else:
        canDlc.config(text='Err')

canData.bind('<KeyRelease>', update_can_message)

def update_can_id(event):
    canId.delete(3, END)

canId.bind('<KeyRelease>', update_can_id)

# Execute Tkinter
window.mainloop()