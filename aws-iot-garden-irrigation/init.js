load('api_aws.js');
load('api_gpio.js');
load('api_timer.js');
load('api_sys.js');
load('api_adc.js');

load('custom.js');

let PUMP_PIN = 4;
let DEFAULT_DEEP_SLEEP_INTERVAL =  60 * 60;

let irrigationinprogress = false;

let sleepinterval = DEFAULT_DEEP_SLEEP_INTERVAL;

GPIO.set_mode(PUMP_PIN, GPIO.MODE_OUTPUT);



function reportState() {
  let state1 = {
                heartbeat:Timer.now(),
                voltagescale: ADC.read(0)
               };
  
  print('Reporting state:', JSON.stringify(state1));
  
  AWS.Shadow.update(0, {
    "reported": state1,
  });
}

let BUTTON_GPIO = 0;
let BUTTON_PULL = GPIO.PULL_UP;
let BUTTON_EDGE = GPIO.INT_EDGE_POS;

AWS.Shadow.setStateHandler(function(ud, ev, reported, desired, reported_md, desired_md) {
  print('Event:', ev, '('+AWS.Shadow.eventName(ev)+')');

  if (ev === AWS.Shadow.CONNECTED) {
     reportState();
    return;
  }

  if (ev !== AWS.Shadow.GET_ACCEPTED && ev !== AWS.Shadow.UPDATE_DELTA) {
    return;
  }

  print('Reported state:', JSON.stringify(reported));
  print('Desired state :', JSON.stringify(desired));
  
  let enabled = desired.enabled !== undefined ? desired.enabled: false;
  sleepinterval = desired.sleepinterval !== undefined ? desired.sleepinterval: sleepinterval;
  
  print ("System is enabled", enabled);
  
  if (enabled && desired.waternow !== undefined)
  {
    //We need to start the pump
    let startwater = (desired.waternow && (!irrigationinprogress)) ;
    if (startwater)
    {
      let dosagesize = 10; //Default value
      if (desired.dosagesize !== undefined)
      {
        dosagesize = desired.dosagesize;
      }
      Timer.set(0, false, function() {
        startpump();
      },null);
      
      //pump starts at the end of the function
      //You may need to increase timeout by a few seconds to compensate
      Timer.set( dosagesize * 1000 , false, function() {
        stoppump();
      },null);
      
    }
  }
}, null);


function startpump()
{
    print (Timer.now());
    print ("Updating Shadow")
    irrigationinprogress = true;
    //Set watering flag to false to stop another trigger
    AWS.Shadow.update(0, {
      reported: {
        lastwatering: Timer.now(),
      },
      desired: {
        waternow: false,
	    sleepinterval : DEFAULT_DEEP_SLEEP_INTERVAL,
      },
    });
    print ("Starting the Pump");
    GPIO.write(PUMP_PIN, 1);
}

function stoppump(dosagesize)
{
    print (Timer.now());
    print ("Stopped the Pump");
    GPIO.write(PUMP_PIN, 0);
    irrigationinprogress = false;
}

Timer.set(30 * 1000, true, function() {
  if (!irrigationinprogress)
  {
    print ("Ready to go to sleep for ", sleepinterval);
    ExtraFuncs.deepsleep(sleepinterval * 1000* 1000);
  }
  else
  {
    print ("Need to wait before going to sleep");
  }
}, null);

