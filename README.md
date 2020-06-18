# FabLab Irregation System

## Design

C++ for a Microprocessor that includes an embedded http server.

The http server displays controls for N zones, where each zone has:

- turn on time
- turn on duration

```
Valve1 \
Valve2  --- Microprocessor <==> Web Interface  
ValveN /
```


| **Web Interface**                 | **Microprocessor** (ESP32)                   |
|-----------------------------------|----------------------------------------------|
| Set per zone:                     | Receives:                                    |
| [ ] start times                   | start times                                  |
| [ ] duration of cicle, max 1 hour | Duration or cicle                            |
|                                   | Override button                              |
| Displays                          |                                              |
| [x] time of day                   |                                              |
| [x] Humidity                      |                                              |
| [x] Temperature                   |                                              |
| [ ] current zone programs stored  |                                              |
| [ ] override button               |                                              |
| [ ] current zone state  (ON/OFF)  | Updates/sends current situation:             |
|                                   | [x]Time of day;                              |
|                                   | [ ]State of zone (ON/OFF)                    |
|                                   | [x]sensor information (humidity/temperature) |


## Web server methods

| **Endpoint**    | **Purpose**                             |
|-----------------|-----------------------------------------|
| index.html      | Serves up main index page               |
| /stats          | returns temp/humidity/time/etc          |
| /getSettings    | get settings                            |
| /saveSettings   | save settings                           |
| /stop?pin={pin} | stop (override) currently running valve |

## Core Logic

**State Problem**  
When the microcontroller loses power, we lose state. We therefore need to persist settings to disk.

- Settings are *read* from disk on startup
- Settings are *saved* each time a user changes settings in the web ui
- Settings are kept in memory and we check against them

We first considered using a simple csv file to commit settings:

```csv
zonenumber,pin number, start hours, run seconds
zone1, 1, , 1234
zone2, 2, 20200101 00:00:00.0000, 123
zone3, 3, 20200101 00:00:00.0000, 1234
```

but this limits our ability to introduce scheduling with one zone coming on and off multiple times per day.
We therefore switched to a simple json conventions.

Stats:

```json
{
  "temp": 55,
  "humidity": 68,
  "time": "20200101 10:20:00"
}
```

Settings: 

```json
{
    "zones": [
        { "name": "zone1", "pin": "1", "start": "00:01:00", "runSeconds": 3000 }, 
        { "name": "zone2", "pin": "2", "start": "00:02:00", "runSeconds": 4000 }, 
        { "name": "zone3", "pin": "3", "start": "00:03:00", "runSeconds": 5000 }
    ]
}
```

TODO: it might be better to combine these:

```json

{
    "stats": { "temp": 55, "humidity": 68, "time": "20200101 10:20:00" },
    "zones": [
        { "name": "zone1", "pin": "1", "start": "00:01:00", "runSeconds": 3000 }, 
        { "name": "zone2", "pin": "2", "start": "00:02:00", "runSeconds": 4000 }, 
        { "name": "zone3", "pin": "3", "start": "00:03:00", "runSeconds": 5000 }
    ]
}

```


## Startup Logic Flow

- Read from csv file 
- Keep info in memory while checking times in a loop ("should I be running now?") 


## TODO

- since we need to return zone state (on/off) in the server polling, we might want to have one single
    json format for both zones & stats 
- update js to load settings from json 
- update js to save settings back to server (need to fix the form submission / json constrution via JSON.stringify))
- fix server logic / endpoints to speak json

