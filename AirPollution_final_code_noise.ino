
#define VERSION "V3.00.02"

// Do not remove the include below
#include "AirPollution.h"

// buffer to store above readings
struct sensors_data data_15_sec[PRIMARY_BUF_COUNT];
struct sensors_data data_10_min[SEC_BUF_COUNT];

short int idx_data_15_sec = 0;
short int idx_data_10_min = 0;
short int hist_start_idx = 0;



bool hist_idx_saved = false;

static bool wifi_connected = false;
static uint64_t previous_ts=0;

/*
   flag is true if no network condition (i.e., failed to publish data to server)
   retry_wifi() takes overs to re establish the WiFi

*/
static bool no_network = false;

static uint32_t last_om2m_pub = 0;
static uint32_t last_thngspk_pub = 0;
static uint32_t last_retry_wifi = 0;

static uint32_t retry_wifi_interval = RETRY_WIFI_INTERVAL;

#ifdef DEBUG
static uint32_t loop_start_milli = 0;
static uint32_t loop_end_milli = 0;
#endif

int count_hw = 0;
int pub_status_om2m2 = 0;
int pub_status_thngspk = 0;
//int cal_status_avg = 0;
int flag = 0;
void print_dm_stats();
void read_data_from_hw();
int calc_10_min_avg();
int retry_wifi();

bool is_om2m_pub_due(unsigned long milli);
bool is_thngspk_pub_due(unsigned long milli);

void setup() {
  Serial.begin(115200);
  Serial.print("***** Setup (from Eclipse ");
  Serial.print(VERSION);
  Serial.println(") *****");

#ifdef DEBUG
  loop_start_milli = millis();
  debug_info(get_date_timestamp_str(get_timestamp(loop_start_milli), Timeoffset)
             + ": Setup start...\n");
#endif

  Serial.println(" ");
  delay(500);          // yield for a moment

  print_dm_stats();

#ifdef DEBUG
  loop_start_milli = millis();
  debug_info(get_date_timestamp_str(get_timestamp(loop_start_milli), Timeoffset)
             + ": WiFi start...\n");
#endif


  nw_setup();
  wifi_connected = nw_start(STARTUP_WIFI_TIMEOUT, STARTUP_WIFI_RETRY_DELAY);

  //check if connected
  if (wifi_connected)
    Serial.println("\n WiFi connected.");
  else
    Serial.println("\n WiFi not connected.");

#ifdef DEBUG
  loop_start_milli = millis();
  debug_info(get_date_timestamp_str(get_timestamp(loop_start_milli), Timeoffset)
             + ": WiFi end.\n");
#endif

  if (wifi_connected) {
    // syn the timestamp
    sync_time();
    uint64_t ts = get_timestamp();
    Serial.println(
      "Local timestamp: " + get_timestamp_str(ts, Timeoffset) + "\t"
      + "Date: " + get_datestamp_str(ts, Timeoffset));
    //Serial.print("timestamp uint64_t: ");
    String time_stamp = get_date_timestamp_str(ts, Timeoffset);
    Serial.println("UTC :-: Time :-: Date: " + time_stamp); // For UTC, take timeoffset as 0
    Serial.println("");
  }


  /*
     setup for various sensors connected to hw
  */
#ifdef DEBUG
  loop_start_milli = millis();
  debug_info(get_date_timestamp_str(get_timestamp(loop_start_milli), Timeoffset)
             + ": Setup h/w start...\n");
#endif
  hw_setup_aht();
  hw_setup_noise();
  hw_setup_sds();

#ifdef DEBUG
  loop_start_milli = millis();
  debug_info(get_date_timestamp_str(get_timestamp(loop_start_milli), Timeoffset)
             + ": Setup h/w end.\n");
#endif
  /*
     setup for ThingSpeak
  */
  pub_setup_thingspeak();


  /*
     disconnect WiFi
  */
  //TODO: re-look later
  //  debug_info("Disconnecting WiFi...\n");
  //  nw_stop();
  //  wifi_connected = nw_is_connected();
  //  wifi_connected ? debug_info("failed to disconnect WiFi!\n") : debug_info("disconnected WiFi.\n");;

#ifdef DEBUG
  loop_start_milli = millis();
  debug_info(get_date_timestamp_str(get_timestamp(loop_start_milli), Timeoffset)
             + ": Setup end.\n");
#endif
  Serial.println("***** Endof setup *****");

  delay(30000);// test case2

}

void loop() {

#ifdef DEBUG
  loop_start_milli = millis();
  debug_info(get_date_timestamp_str(get_timestamp(loop_start_milli), Timeoffset)
             + ": Loop start...\n");
#endif
  debug_info("\ncal_status_avg5:");
  debug_info(flag);
  read_data_from_hw();
  unsigned long milli = millis();
  debug_info("check pub due condition...\n");
  bool pub_om2m = is_om2m_pub_due(milli);
  bool pub_thngspk = is_thngspk_pub_due(milli);

  if ((pub_om2m || pub_thngspk) && !no_network) {
    debug_info("Connecting WiFi...\n");
    wifi_connected = nw_start(STARTUP_WIFI_TIMEOUT, STARTUP_WIFI_RETRY_DELAY);
  }
  debug_info("nw_is_connected");
  debug_info(nw_is_connected());

  if (nw_is_connected() && flag)
  {
    debug_info("pub_10_min_avg_data3...\n");
    pub_10_min_data(data_10_min, hist_start_idx, idx_data_10_min);
     pub_10_min_thingspeak(data_10_min, hist_start_idx, idx_data_10_min);
    flag = 0;
    hist_idx_saved = false;
  }

  if (pub_om2m) {
    debug_info("pub_15_sec_data...\n");
    pub_status_om2m2 = pub_15_sec_data(data_15_sec, idx_data_15_sec - 1, 0,
       false);
  }

  if (pub_thngspk)
    pub_status_thngspk = pub_thingspeak(data_15_sec, idx_data_15_sec - 1);

  if (pub_status_om2m2 == -1) {
    no_network = true;
    // save 10 min average index
    if (!hist_idx_saved) {
      hist_start_idx = idx_data_10_min;
      hist_idx_saved = true;

    }
    if (calc_10_min_avg())
    {
      flag = 1;
      // hist_start_idx = idx_data_10_min;
      debug_info("\nIndex of 10 min:");
      debug_info(idx_data_10_min);
      debug_info("\n");

    }

    debug_info("\ncal_status_avg2:");
    debug_info(flag);
    // calc_10_min_avg(count_hw, idx_data_15_sec );

    retry_wifi();
    if (nw_is_connected() && flag == 1) {
      sync_time();
      no_network = false;
      hist_idx_saved = false;
      debug_info("\ncal_status_avg1:");
      debug_info(flag);
      // publish missed data, i.e, 10 min average historical data
      pub_10_min_data(data_10_min, hist_start_idx, idx_data_10_min);
      pub_10_min_thingspeak(data_10_min, hist_start_idx, idx_data_10_min);
      debug_info("//////////////inside////////////");
      flag = 0;
    }

  }

#ifdef DEBUG
  loop_end_milli = millis();
  debug_info(get_date_timestamp_str(get_timestamp(loop_end_milli), Timeoffset)
             + ": Loop ended.\n");
  debug_info("Loop process time: " + String(loop_end_milli - loop_start_milli) + " ms.\n");
#endif

  debug_info("Sleeping for 500 ms!\n");
  delay(500);
}


/*
   check if pub OneM2M is due

*/
unsigned long om2m_pub_interval = TIME_INTERVAL_1_MIN;
bool is_om2m_pub_due(unsigned long milli) {

  if (milli - last_om2m_pub > om2m_pub_interval) {
    debug_info("Time interval : ");
    debug_info(milli);
    last_om2m_pub = milli;
    return true;
  }
  else
    return false;
}

/*
   check if pub ThingSpeak is due

*/
unsigned long thngspk_pub_interval = TIME_INTERVAL_1_MIN;
bool is_thngspk_pub_due(unsigned long milli) {
  if (milli - last_thngspk_pub > thngspk_pub_interval) {
    last_thngspk_pub = milli;
    return true;
  }
  else
    return false;
}

int retry_wifi() {
  //regular interval with incremental back-off try to reconnect WiFi

  unsigned long milli  = millis();
  if (milli - last_retry_wifi > retry_wifi_interval) {

    debug_info(get_date_timestamp_str(get_timestamp(milli), Timeoffset)
               + ": retrying WiFi...");
    wifi_connected = nw_start(STARTUP_WIFI_TIMEOUT, STARTUP_WIFI_RETRY_DELAY);
    last_retry_wifi = millis();
    retry_wifi_interval += retry_wifi_interval * RETRY_WIFI_FACTOR;
    wifi_connected = nw_start(STARTUP_WIFI_TIMEOUT, STARTUP_WIFI_RETRY_DELAY);
  }

  // if successfully connected to WiFi
  if (wifi_connected) {
    debug_info(get_date_timestamp_str(get_timestamp(millis()), Timeoffset)
               + ": WiFi connected");
    no_network = false;
  }
  return 0;
}

void read_data_from_hw() {
  static uint32_t last_ms_read_data_15 = 0;
 
  if (millis() - last_ms_read_data_15 > TIME_INTERVAL_15_SEC) {
    last_ms_read_data_15 = millis();

    uint64_t time_stamp = get_timestamp();
    String timestamp = get_date_timestamp_str(time_stamp, 0);
    data_15_sec[idx_data_15_sec % PRIMARY_BUF_COUNT].time_stamp = time_stamp;

    debug_info(get_date_timestamp_str(time_stamp, Timeoffset)
               + ": read_data_from_hw...\n");
    /*
        read data from hardware
    */
  
    hw_read_sds(&data_15_sec[idx_data_15_sec % PRIMARY_BUF_COUNT].pm25,
                &data_15_sec[idx_data_15_sec % PRIMARY_BUF_COUNT].pm25_cal,
                &data_15_sec[idx_data_15_sec % PRIMARY_BUF_COUNT].pm10,
                &data_15_sec[idx_data_15_sec % PRIMARY_BUF_COUNT].pm10_cal );

    hw_read_aht(&data_15_sec[idx_data_15_sec % PRIMARY_BUF_COUNT].temp,
                &data_15_sec[idx_data_15_sec % PRIMARY_BUF_COUNT].temp_cal,
                &data_15_sec[idx_data_15_sec % PRIMARY_BUF_COUNT].rh,
                &data_15_sec[idx_data_15_sec % PRIMARY_BUF_COUNT].rh_cal);

    hw_read_noise(&data_15_sec[idx_data_15_sec % PRIMARY_BUF_COUNT].dbValue,
                  &data_15_sec[idx_data_15_sec % PRIMARY_BUF_COUNT].dbValue_cal);

    data_15_sec[idx_data_15_sec % PRIMARY_BUF_COUNT].aqi = compute_index_aqi(
          data_15_sec[idx_data_15_sec % PRIMARY_BUF_COUNT].pm25_cal,
          data_15_sec[idx_data_15_sec % PRIMARY_BUF_COUNT].pm10_cal);
  

    debug_info("AQI: ");
    debug_info(data_15_sec[idx_data_15_sec % PRIMARY_BUF_COUNT].aqi);

    data_15_sec[idx_data_15_sec % PRIMARY_BUF_COUNT].aql = aql(
          data_15_sec[idx_data_15_sec % PRIMARY_BUF_COUNT].aqi);

    debug_info("AQL: ");
    debug_info(data_15_sec[idx_data_15_sec % PRIMARY_BUF_COUNT].aql);


     data_15_sec[idx_data_15_sec % PRIMARY_BUF_COUNT].aqlmp = aqi_mp(
          data_15_sec[idx_data_15_sec % PRIMARY_BUF_COUNT].pm25_cal,
          data_15_sec[idx_data_15_sec % PRIMARY_BUF_COUNT].pm10_cal);


    debug_info("AQL_mp: ");
    debug_info(data_15_sec[idx_data_15_sec % PRIMARY_BUF_COUNT].aqlmp);

    Serial.print("count of hardware outputs:  ");
    Serial.println(count_hw++);


    if (idx_data_15_sec == PRIMARY_BUF_COUNT)
    { idx_data_15_sec = 0;
      count_hw = 0;
    }
    else
      idx_data_15_sec++;
  }
}

int calc_10_min_avg() {

  static uint32_t last_ms_read_data_10 = 0;
  if (millis() - last_ms_read_data_10 > TIME_INTERVAL_10_MIN) {
    last_ms_read_data_10 = millis();

    float sum_temp = 0;
    float sum_rh = 0;
    float sum_temp_cal = 0;
    float sum_rh_cal = 0;
    float sum_pm25 = 0.0;
    float sum_pm10 = 0.0;
    float sum_pm25_cal=0.0;
    float sum_pm10_cal=0.0;
    float sum_dbValue =0.0;
    float sum_dbValue_cal =0.0;
    int latest_buf_idx =  idx_data_15_sec;


    int max = PRIMARY_BUF_COUNT;
    int max_buf = SEC_BUF_COUNT;

    debug_info("Compute 10 min data...\n");
    debug_info("count_hw: ");
    debug_info(count_hw);

    int no_records = count_hw;

    int start_idx = latest_buf_idx + (max - no_records) + 1;
    int end_idx = max + latest_buf_idx;

    debug_info("no_records: " + String(no_records) + "\n");
    debug_info("start_idx: " + String(start_idx) + "\n");
    debug_info("end_idx: " + String(end_idx) + "\n");
    debug_info("max_record_count: " + String(max) + "\n");
    debug_info("before for 10 min loop");



    for (int i = start_idx; i <= end_idx; i++) {


      //TODO: consider only valid value for average


      sum_temp += data_15_sec[i % max].temp;
      sum_rh += data_15_sec[i % max].rh;

      sum_temp_cal += data_15_sec[i % max].temp_cal;
      sum_rh_cal += data_15_sec[i % max].rh_cal;


      sum_pm25 += data_15_sec[i % max].pm25;
      sum_pm10 += data_15_sec[i % max].pm10;

      sum_pm25_cal += data_15_sec[i % max].pm25_cal;
      sum_pm10_cal += data_15_sec[i % max].pm10_cal;

      sum_dbValue += data_15_sec[i % max].dbValue;
      sum_dbValue_cal += data_15_sec[i % max].dbValue_cal;

      debug_info("Sum Pm 25");
      debug_info(sum_pm25);
      debug_info("Sum Pm 10");
      debug_info(sum_pm10);


       debug_info("Sum Pm 25 cal");
      debug_info(sum_pm25_cal);
      debug_info("Sum Pm 10 cal");
      debug_info(sum_pm10_cal);


    }

    /*

       Taking out average
    */

    uint64_t time_stamp = get_timestamp();
    data_10_min[idx_data_10_min % max_buf].time_stamp = time_stamp;

    data_10_min[idx_data_10_min % max_buf].pm25 = sum_pm25
        / no_records;
    data_10_min[idx_data_10_min % max_buf].pm25_cal = sum_pm25_cal
        / no_records;
    data_10_min[idx_data_10_min % max_buf].pm10 = sum_pm10
        / no_records;
    data_10_min[idx_data_10_min % max_buf].pm10_cal = sum_pm10_cal
        / no_records;
    data_10_min[idx_data_10_min % max_buf].temp = sum_temp
        / no_records;
    data_10_min[idx_data_10_min % max_buf].temp_cal = sum_temp_cal
        / no_records;
    data_10_min[idx_data_10_min % max_buf].rh = sum_rh
        / no_records;
    data_10_min[idx_data_10_min % max_buf].rh_cal = sum_rh_cal
        / no_records;
    data_10_min[idx_data_10_min % max_buf].dbValue = sum_dbValue
        / no_records;
    data_10_min[idx_data_10_min % max_buf].dbValue_cal = sum_dbValue_cal
        / no_records;
    uint64_t ts = data_10_min[idx_data_10_min].time_stamp;
    Serial.println(
      "Local timestamp: " + get_timestamp_str(ts, Timeoffset) + "\t"
      + "Date: " + get_datestamp_str(ts, Timeoffset));

    debug_info("Sum Temp: ");
    debug_info(data_10_min[idx_data_10_min].temp);
    debug_info("Sum Humidity: ");
    debug_info(data_10_min[idx_data_10_min].rh);
    debug_info("Sum Temp cal: ");
    debug_info(data_10_min[idx_data_10_min].temp_cal);
    debug_info("Sum Humidity cal: ");
    debug_info(data_10_min[idx_data_10_min].rh_cal);
    debug_info("Sum PM25: ");
    debug_info(data_10_min[idx_data_10_min].pm25);
    debug_info("Sum PM10: ");
    debug_info(data_10_min[idx_data_10_min].pm10);


    debug_info("Sum PM25_cal: ");
    debug_info(data_10_min[idx_data_10_min].pm25_cal);
    debug_info("Sum PM10_cal: ");
    debug_info(data_10_min[idx_data_10_min].pm10_cal);


    if (idx_data_10_min == SEC_BUF_COUNT)
    {
      idx_data_10_min = 0;
    }
    else
    {
      idx_data_10_min++;
    }
    return 1;
  }
  else
    return 0;

}

void print_dm_stats() {
  debug_info(
    "sizeof(sensors_data): " + String(sizeof(struct sensors_data))
    + " bytes\n\n");

  debug_info("sizeof(PRIMARY_BUF_COUNT): " + String(PRIMARY_BUF_COUNT));
  debug_info(
    "sizeof(data_15_sec): " + String(sizeof(data_15_sec))
    + " bytes\n\n");

  debug_info("sizeof(SEC_BUF_COUNT): " + String(SEC_BUF_COUNT));
  debug_info(
    "sizeof(data_10_min): " + String(sizeof(data_10_min))
    + "bytes\n\n");
  debug_info(
    "Total: " + String(sizeof(data_15_sec) + sizeof(data_10_min))
    + " bytes\n\n");
}
