#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <float.h>

#define TIME_COEFF 1000 // 1 ms
#define BEACON_QUANTIZER (TIME_COEFF) // Quantize beacon offsets to 1 ms
#define RAND_MAX_NEW 4294967295 //this is just 2*RAND_MAX
#define BEACON_RAND_TIME_MOD (11 * TIME_COEFF) //give us a random number from 0-10ms
#define SUCCESS_LIMIT 100.0f
#define TIME_BETWEEN_ADV 100 // 100 us
//#define PRINT_DEBUG //define for debug prints
//#define PRINT_STATUS //define for status output
//#define PRINT_HITS //print hits


/* Beacon class */
typedef struct Beacon
{
	int b_time;
	int length;
	char active;
	char fail;
	char identified;
	int channel;
} beacon;

/* Beacon initialization */
void init_beacons(beacon* b, int num_beacons, int beacon_send_interval, int beacon_length)
{
	for (int i = 0; i < num_beacons; i++)
	{
		unsigned int scaled = RAND_MAX_NEW*(double)rand() / ((double)RAND_MAX + 1);
		b[i].b_time = ((scaled / BEACON_QUANTIZER)* BEACON_QUANTIZER) % beacon_send_interval; // Time until beacon is active again
		b[i].length = beacon_length; // Packet length, indicates how many more time units the beacon is still active
		b[i].active = 0; // Active flag
		b[i].fail = 0; // Beacon failed flag
		b[i].identified = 0; // Beacon identified flag
		b[i].channel = 0; // 0-2 primary advertising
		printf("next active time: %d\n", b[i].b_time); //next active time in us
	}
}

int main(int argc, char *argv[])
{
	int scan_window = 10;	//ms
	int scan_interval = 1000; //ms
	int beacon_tx_interval = 500; //ms
	int NUM_BEACONS = 1;
	int BEACON_SCAN_TIME = TIME_COEFF*scan_window; /* result in us */
	int BEACON_SEND_INTERVAL = TIME_COEFF*beacon_tx_interval; /* result in us */
	int BEACON_SCAN_INTERVAL = TIME_COEFF*scan_interval; /* result in us */
	int NUM_RUN = 100;
	int BEACON_LENGTH = 368; //us
	beacon beacons[NUM_BEACONS];
	FILE *fp;

	fp = fopen("data.csv","a");
	fprintf(fp,"Num Beacons,Scan Window ms,Scan Interval ms,Beacon Tx Interval ms,Avg s,Max s,Min s\n");
	fflush(fp);

	srand(time(NULL));

  for (scan_window=25;scan_window>0;scan_window--)
	{
		float sum_time = 0.0f;
		float max_time = 0.0f;
		float min_time = FLT_MAX;
		BEACON_SCAN_TIME = TIME_COEFF*scan_window; /* result in us */
		printf("******\nscan_window = %d\n",scan_window);
		fflush(stdout);
		for (int c = 0; c < NUM_RUN; c++)
		{
			printf("----------------------------------------------\n");
			printf("Run #%d\n", c+1);
			fflush(stdout);
			int beacons_sent = 0;
			int beacons_hit = 0;
			int total_identified = 0;
			int total_beacons_active = 0;
			int beacon_scan_time = 0;
			int scan_channel = 0; // Primary advertising channels 0-2, secondary 3-39
			int beac_chan = 0;
			int fail = 0;
			#ifdef PRINT_DEBUG
			int scan_active = 1;
			#endif

			init_beacons(beacons, NUM_BEACONS, BEACON_SEND_INTERVAL, BEACON_LENGTH);
			uint32_t t = 0;
			while (1)
			{
				total_beacons_active = 0;
				#ifdef PRINT_DEBUG
				if (beacon_scan_time > BEACON_SCAN_TIME && scan_active == 1)
				{
					//was inactive, now active
					printf("Time: %0.3fs. Scan ended on ch %d.\n",(float)t / (float)(TIME_COEFF),scan_channel);
					scan_active = 0;
				}
				#endif

				for (int i = 0; i < NUM_BEACONS; i++)
				{
					beacon* b = &beacons[i];

					if (b->b_time == 0) // Beacon is beaconing
					{
						if (b->length > 0) // Beacon has packet to send
						{
							#ifdef PRINT_DEBUG
							if (b->active == 0)
							{
								//was inactive, now active
								printf("Time: %0.3fs. Beacon #%d active on ch %d.\n",(float)t / (float)(TIME_COEFF),i,b->channel);
							}
							#endif
							b->active = 1;
							b->length--;

							if (beacon_scan_time > BEACON_SCAN_TIME || b->channel != scan_channel)
							{
								b->fail = 1; // Whole packet could not be scanned
							}

						}
						else // Beacon has sent the packet completely
						{
							beac_chan = b->channel;
							fail = b->fail;
							//printf("Time: %0.3f ms packet #%d sent on channel: %d. Current scan channel %d, fail %d, scan time %d \n", (float)t / (float)(TIME_COEFF), b->packetNr, beac_chan, scan_channel, fail, beacon_scan_time);

							if (b->fail)
							{
								//printf("%d. %d. BEACON FAILED\n", t, i);
							}

							else if (beacon_scan_time < BEACON_SCAN_TIME && scan_channel == beac_chan) // Beacon did not fail, packet is completely sent and scan was active throughout the packet
							{
								beacons_hit++;
								if (b->identified == 0)
								{
									total_identified++;
									#ifdef PRINT_STATUS
										printf("Time: %0.3fs. Beacon #%d OK. %d/%d identified. \n", (float)t / (float)(1000 * TIME_COEFF), i, total_identified, NUM_BEACONS);
									#endif
								}
								b->identified = 1;
								#ifdef PRINT_HITS
								printf("Time: %0.3fs. Beacon #%d HIT.\n", (float)t / (float)(1000 * TIME_COEFF), i);
								#endif
							}
							beacons_sent++;

							b->active = 0;
							#ifdef PRINT_DEBUG
								//now inactive
								printf("Time: %0.3fs. Beacon #%d inactive on ch %d.\n",(float)t / (float)(TIME_COEFF),i,b->channel);
							#endif

							// Channel&packet switching
							if (b->channel == 2) // All three channels are done - wait for next beacon interval
							{
								b->channel = 0;
								//advertisements include a random 0-10ms delay
								int offset = ((rand() / BEACON_QUANTIZER) * BEACON_QUANTIZER) % BEACON_RAND_TIME_MOD;
								// The beacon interval is between the beginning of beacons. Subtract off the time
								//  from this last beacon.
								b->b_time = BEACON_SEND_INTERVAL + offset - (3 * (BEACON_LENGTH)) - (2 * TIME_BETWEEN_ADV);
								b->length = BEACON_LENGTH;
								b->fail = 0;
							}
							else // Next channel
							{
								b->channel++;
								b->b_time = TIME_BETWEEN_ADV;
								b->length = BEACON_LENGTH;
								b->fail = 0;
							}
						}
					}
					else if (b->b_time > 0) // Beacon is not yet active, decrement the time left until beaconing starts
					{
						b->b_time--;
						b->active = 0;
					}

					if (b->active && b->channel == scan_channel)
					{
						total_beacons_active++; // Number of beacons that are currently active on the scan channel
						//printf("%d. %i. beacon!\n", t, i);
					}

				}

				if (total_beacons_active > 1)
				{
					// Collision
					for (int j = 0; j < NUM_BEACONS; j++)
					{
						if (beacons[j].active && beacons[j].channel == scan_channel)
						{
							beacons[j].fail = 1;
							//printf("Packet collision, channel %d (scan channel %d), packet %d, time %d \n", beacons[j].channel, scan_channel, beacons[j].packetNr, t);
						}
					}

				}

				beacon_scan_time++;

				/* Regular scanning, change channel between 0-2 after each interval */
				if (beacon_scan_time >= BEACON_SCAN_INTERVAL)
				{
					beacon_scan_time = 0;
					if (scan_channel >= 2)
					{
						scan_channel = 0;
					}
					else
					{
						scan_channel++;
					}
					#ifdef PRINT_DEBUG
						//Just switched channel for scan
						printf("Time: %0.3fs. Next scanning ch %d.\n",(float)t / (float)(TIME_COEFF),scan_channel);
						scan_active = 1;
					#endif
				}

				t++;
				if ((total_identified == NUM_BEACONS || ((float)total_identified * 100.0f / (float)NUM_BEACONS) > SUCCESS_LIMIT))
				{
					break;
				}
			}//end while

		printf("num_beacons_sent: %d, num_beacons_hit: %d, percent: %lf\n", beacons_sent, beacons_hit, (float)beacons_hit * 100.0f / (float)beacons_sent);
		printf("total time taken: %lf s, identified percent: %lf\n", (float)t / (float)(1000 * TIME_COEFF), (float)total_identified * 100.0f / (float)NUM_BEACONS);
		sum_time += (float)t / (float)(1000 * TIME_COEFF);
		if ((float)t / (float)(1000 * TIME_COEFF) > max_time)
		{
			max_time = (float)t / (float)(1000 * TIME_COEFF);
		}
		if ((float)t / (float)(1000 * TIME_COEFF) < min_time)
		{
			min_time = (float)t / (float)(1000 * TIME_COEFF);
		}
		fflush(stdout);
	} //end of run

	printf("avg results over %d runs: %lf s\n", NUM_RUN, sum_time / (float)NUM_RUN);
  printf("max time: %1f seconds, min time: %1f seconds\n", max_time, min_time);
	fflush(stdout);

	//save to file?
	fprintf(fp,"%d,%d,%d,%d,%1f,%1f,%1f\n",
		NUM_BEACONS,scan_window,scan_interval,beacon_tx_interval,
		sum_time / (float)NUM_RUN, max_time, min_time);
	fflush(fp); //keep file updated
} //end outer iteration

fclose(fp);
	return 0;
}
