load('api_aws.js');
load('api_gpio.js');
load('api_timer.js');



let PUMP_PIN = 4;
GPIO.set_mode(PUMP_PIN, GPIO.MODE_OUTPUT)
GPIO.set_pull(PUMP_PIN, GPIO.PULL_DOWN)

let state = {
  heartbeat: 0,
};


function reportState() {
  let state1 = {heartbeat:Timer.now()};
  
  print('Reporting state:', JSON.stringify(state1));
  
  AWS.Shadow.update(0, {
    "reported": state1,
  });
}

let BUTTON_GPIO = 0;
let BUTTON_PULL = GPIO.PULL_UP;
let BUTTON_EDGE = GPIO.INT_EDGE_POS;

/*
GPIO.set_button_handler(
  BUTTON_GPIO, BUTTON_PULL, BUTTON_EDGE, 50 ,
  function(pin, ud) {
    let updRes = AWS.Shadow.update(0, {
      desired: {
        welcome: state.welcome + 1,
      },
    });
    print("Click! Updated:", updRes);
  }, null
);
*/

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

  /*
   * Here we extract values from previosuly reported state (if any)
   * and then override it with desired state (if present).
   */
  //updateState(reported);
  //updateState(desired);

  //print('New state:', JSON.stringify(state));

  /*if (ev === AWS.Shadow.UPDATE_DELTA) {
    reportState();
  }*/
  
  if (desired.waternow !== undefined)
  {
    //We need to start the pump
    let startwater = desired.waternow;
    if (desired.waternow)
    {
      let dosagesize = 10; //Default value
      if (desired.dosagesize !== undefined)
      {
        dosagesize = desired.dosagesize;
      }
      waternow(dosagesize);
    }
  }
}, null);


function waternow(dosagesize){
    let updRes = AWS.Shadow.update(0, {
      desired: {
        waternow: false,
      },
    }); 
    
    GPIO.write(PUMP_PIN, 1);
    
 /*   Timer.set(dosagesize * 1000, false, function() {
      GPIO.write(PUMP_PIN, 0);
      let updRes = AWS.Shadow.update(0, {
      reported: {
        lastwatering: Timer.now(),
      },
    }); 
    
    }, null);
   */ 
}