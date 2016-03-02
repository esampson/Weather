#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <RF24/RF24.h>
#include <my_global.h>
#include <mysql.h>
#include <cmath>

RF24 radio(25,0,BCM2835_SPI_SPEED_8MHZ);

// Topology
const uint64_t pipes[2] = { 0xABCDABCD71LL, 0x544d52687CLL };              // Radio pipe addresses for the 2 nodes to communicate.
char message[22];
char LValue[4][5] = {"Min", "Low", "High", "Max"};

struct connection_details
{
    char *server;
    char *user;
    char *password;
    char *database;
};

struct Data {
  uint16_t    level;
  uint16_t    sensor;
  uint16_t    flags;
  uint16_t    trash;
  float       temperature;
  float       humidity;
  float       pressure;
  float       moisture;
};

MYSQL* mysql_connection_setup(struct connection_details mysql_details)
{
     // first of all create a mysql instance and initialize the variables within
    MYSQL *connection = mysql_init(NULL);

    // connect to the database with the details attached.
    if (!mysql_real_connect(connection,mysql_details.server, mysql_details.user, mysql_details.password, mysql_details.database, 0, NULL, 0)) {
      printf("Conection error : %s\n", mysql_error(connection));
      exit(1);
    }
    return connection;
}

MYSQL_RES* mysql_perform_query(MYSQL *connection, char *sql_query)
{
   // send the query to the database
   if (mysql_query(connection, sql_query))
   {
      printf("MySQL query error : %s\n", mysql_error(connection));
      exit(1);
   }

   return mysql_use_result(connection);
}

void finish_with_error(MYSQL *con)
{
  fprintf(stderr, "%s\n", mysql_error(con));
  mysql_close(con);
  exit(1);
}

int main(int argc, char** argv){

  Data data;
  printf("RF24 Reciever started...\n");
  radio.begin();
  radio.setAutoAck(1);                    	// Ensure autoACK is enabled
  radio.enableAckPayload();               	// Allow optional ack payloads
  radio.setRetries(0,15);                 	// Smallest time between retries, max no. of retries
  radio.setPALevel(RF24_PA_MAX);
  radio.openWritingPipe(0x16f312b711);        	// Both radios listen on the same pipes by default, and switch when writing
  radio.openReadingPipe(1,0x16f312b712);
  radio.openReadingPipe(2,0x16f312b713);
  radio.startListening();                 	// Start listening
  radio.setDataRate(RF24_250KBPS);
  radio.printDetails();                   	// Dump the configuration of the rf unit for debugging
  radio.setPayloadSize(24);

  MYSQL *conn;					// the connection
  MYSQL_RES *res;				// the results
  MYSQL_ROW row;				// the results row (line by line)

  struct connection_details mysqlD;
  mysqlD.server = "localhost";  		// where the mysql database is
  mysqlD.user = "root";				// the root user of mysql
  mysqlD.password = "kBua!zDUyuq&46E"; 		// the password of the root user in mysql
  mysqlD.database = "weather";			// the databse to pick

  while (1){
    uint8_t pipeNo;
    while( radio.available(&pipeNo)){
      radio.read( &data, sizeof(Data) );
      bool tFlag = 0;
      bool hFlag = 0;
      bool pFlag = 0;
      bool mFlag = 0;
      int Flags = data.flags;
      if (Flags > 7){
        mFlag = 1;
        Flags = Flags - 8;
      }
      if (Flags > 3){
        pFlag = 1;
        Flags = Flags - 4;
      }
      if (Flags > 1){
        hFlag = 1;
        Flags = Flags - 2;
      }
      if (Flags > 0){
        tFlag = 1;
      }
      radio.writeAckPayload(pipeNo,&pipeNo,1);
      printf("Sensor: %d\n  Level: %s\n",data.sensor,LValue[data.level]);
      if (tFlag) printf("  Temp: %.1f\n",data.temperature);
      if (hFlag) printf("  Humidity: %.1f\n",data.humidity);
      if (pFlag) printf("  Pressure: %.3f\n",data.pressure);
      if (mFlag) printf("  Moisture: %.2f\n",data.moisture);
      // connect to the mysql database
      conn = mysql_connection_setup(mysqlD);

      // assign the results return to the MYSQL_RES pointer
      char r2[] = "SELECT TEMPERATURE, HUMIDITY, PRESSURE, MOISTURE, \
             TIMESTAMPDIFF(SECOND,(SELECT TIME FROM Sensor_Entry \
             WHERE SENSOR=%d ORDER BY TIME DESC LIMIT 1),\
             CURRENT_TIMESTAMP) as DELTAT FROM Sensor_Entry WHERE \
             SENSOR=%d ORDER BY TIME DESC LIMIT 1;";
      char request[255];
      if (data.sensor == 0) sprintf (request,r2,5,5);
      else sprintf (request,r2,data.sensor,data.sensor);
      res = mysql_perform_query(conn, request);

      while ((row = mysql_fetch_row(res)))
      {
         unsigned long *lengths;
         lengths = mysql_fetch_lengths(res);

         char tbuff [10];
         char hbuff [10];
         char pbuff [10];
         char mbuff [10];
         char dbuff [10];
         sprintf (tbuff, "%.*s ", (int) lengths[0], row[0] ? row[0] : "NULL");
         sprintf (hbuff, "%.*s ", (int) lengths[1], row[1] ? row[1] : "NULL");
         sprintf (pbuff, "%.*s ", (int) lengths[2], row[2] ? row[2] : "NULL");
         sprintf (mbuff, "%.*s ", (int) lengths[3], row[3] ? row[3] : "NULL");
         sprintf (dbuff, "%.*s ", (int) lengths[4], row[4] ? row[4] : "NULL");
         double tval, hval, pval, mval, dval, dtc, dhc, dpc, dmc;
         sscanf (tbuff,"%lf", &tval);
         sscanf (hbuff,"%lf", &hval);
         sscanf (pbuff,"%lf", &pval);
         sscanf (mbuff,"%lf", &mval);
         sscanf (dbuff,"%lf", &dval);
         dtc = 0;
         dhc = 0;
         dpc = 0;
         dmc = 0;
         if (dval > 10) {
           dtc = (data.temperature - tval) / dval;
           dhc = (data.humidity - hval) / dval;
           dpc = (data.pressure - pval) / dval;
           dmc = (data.moisture - mval) / dval;
           if (tFlag) printf("  Delta T: %lf\n",dtc);
           if (hFlag) printf("  Delta H: %lf\n",dhc);
           if (pFlag) printf("  Delta P: %lf\n",dpc);
           if (mFlag) printf("  Delta M: %lf\n",dmc);
           char request[255], r2[255];
           strcpy(request, "INSERT INTO weather.Sensor_Entry (ID, SENSOR, TIME");
           if (tFlag == 1 && abs(dtc) < .009) strcat(request, ", TEMPERATURE, DTC");
           if (hFlag == 1 && abs(dhc) < .034) strcat(request, ", HUMIDITY, DHC");
           if (pFlag == 1 && abs(dpc) < .00025) strcat(request, ", PRESSURE, DPC");
           if (mFlag == 1 && abs(dmc) <.21) strcat(request, ", MOISTURE, DMC");
           strcat(request, ") VALUES (NULL, ");
           sprintf(r2, "%d, CURRENT_TIMESTAMP", data.sensor);
           strcat(request, r2);
           if (tFlag == 1 && abs(dtc) < .009) {
             sprintf(r2,", %.1f, %lf",data.temperature,dtc);
             strcat(request, r2);
           }
           if (hFlag == 1 && abs(dhc) < .034) {
             sprintf(r2,", %.1f, %lf",data.humidity,dhc);
             strcat(request, r2);
           }
           if (pFlag == 1 && abs(dpc) < .00025) {
             sprintf(r2,", %.3f, %lf",data.pressure,dpc);
             strcat(request, r2);
           }
           if (mFlag == 1 && abs(dmc)<.21) {
             sprintf(r2,", %f, %lf",data.moisture,dmc);
             strcat(request, r2);
           }
           strcat(request, ")");
           mysql_free_result(res);
           if (data.sensor != 0) {
             if (mysql_query(conn, request)) {
               finish_with_error(conn);
             }
           }
           else {
             printf("%s\n",request);
             printf("%i\n",sizeof(data));
           }
        }
      }
      /* clean up the database result set */
      //mysql_free_result(res);
      /* clean up the database link */
      mysql_close(conn);
    }
    usleep(1000);
  }
}
