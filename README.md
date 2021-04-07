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

![Screenshot_20210403_191707_com microsoft emmx](https://user-images.githubusercontent.com/67831815/113913158-491a6d00-97dc-11eb-8e98-ac144ba9172a.jpg)
![Screenshot_20210403_191733_com microsoft emmx](https://user-images.githubusercontent.com/67831815/113913161-49b30380-97dc-11eb-82fa-cdd1c6fd29c1.jpg)


**Schematic:**
![Schematic_Charge controller EV_2021-04-07](https://user-images.githubusercontent.com/67831815/113912833-e2954f00-97db-11eb-976a-2fece1691816.png)
