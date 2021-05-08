# Fiorino charger controller

Controls the charger in the car with a pwm signal according to the state of the battery which is parsed from the RS232 port in the car.

- Over voltage protection
- Under voltage current throttling
- Temperature protection & throttling
- Logs data from every charging session to a .csv file

**Webpage:**
1. Connect to wifi network "Fiorino"
2. Open in browser: "http://fiorino.local" or "http://192.168.4.22"

- Supports over-the-air firmware update
- Allows downloading of .csv logfiles

https://github.com/Wim426F/Fiorino_webserver

![frontpage](https://user-images.githubusercontent.com/67831815/113913464-a6162300-97dc-11eb-911e-adc286c77205.PNG)
![rsz_datalog](https://user-images.githubusercontent.com/67831815/113913848-1d4bb700-97dd-11eb-995c-46b96748a67a.png)



**Schematic:**
![Schematic_Charge controller EV_2021-04-07](https://user-images.githubusercontent.com/67831815/113912833-e2954f00-97db-11eb-976a-2fece1691816.png)
