#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <stdlib.h>
//#include <direct.h>
#include <math.h>
#include "mpi.h"
#define PRINT_GRIDS
#define DEBUG_PRINT1
#define DEBUG_PRINT
#define TEXT_PRINT
#define PRINT_GRIDS_INITIAL
#define CELL_SIZE 100

void print_grid(int[][110], int);
static void delay(void);
static void button_press(float x, float y);
static void drawscreen(int);
static void new_button_func(void (*drawscreen_ptr) (void));
void my_setcolor(int);
void endMe(int, char*);

typedef struct connection {
	int x1, y1, p1, x2, y2, p2;
} connection;

typedef struct grid {
	int x, y, value;
} grid;

// Function declarations
connection* add_connection(connection*, int, int, int, int, int, int);
void print_connection(connection*, int, int, int, int, int, int);


using namespace std;

double f(double);

double f(double a)
{
	return (4.0 / (1.0 + a * a));
}


int main(int argc, char** argv)
{
	int n, myid, numprocs, i;
	double mypi, pi, h, sum, x;
	double startwtime = 0.0, endwtime;
	int  namelen;
	char processor_name[MPI_MAX_PROCESSOR_NAME];
	/* */
	FILE* fp;
	FILE* logfile = fopen("mylogfile.log", "r");
	char line[14];
	char* p;
	int r = 5;		// Rows and columns (grid)
	int w = 55;		// Routing tracks in each channel

	int x1, y1, p1, x2, y2, p2;
	int point_count;
	int found;
	int level;
	int prevJ;
	int prevK;

	int startX;
	int startY;
	int endX;
	int endY;
	char filename[40];
	char my_string[40];
	int k, j, z;
	int line_count = 0;
	int conn;
	int	grid_size = 6;
	int grid[110][110];
	int grid_tracks[110][110];
	int tracks_used = 0;
	int total_tracks_used;
	int starting_grid[110][110];

	// Initialize MPI
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &myid);
	MPI_Get_processor_name(processor_name, &namelen);

	cout << "Process #" << myid << " of " << numprocs << " is on " << processor_name << endl;

	// Create variable to hold all connections
	connection my_connections[12100];


#ifdef DEBUG_PRINT
	char CurrentPath[50];
	//GetCurrentPath(CurrentPath);
	cout << CurrentPath << endl;
#endif

	MPI_Barrier(MPI_COMM_WORLD);
	//Get connectivity information from file. Only Host process needs to do this
	if (myid == 0) {
		fprintf(stdout, "1: fcct1\n2: fcct2\n3: fcct3\n4: fcct4\n5: fcct5\nPick file:");
		fflush(stdout);
		if (scanf("%d", &n) != 1) {
			fprintf(stdout, "No number entered; quitting\n");
			endMe(myid, processor_name);
		}
		else {
			fprintf(stdout, "you entered: %d \n", n);
			switch (n) {
			case 1:
				sprintf(filename, "%s", "fcct1.txt");
				break;
			case 2:
				sprintf(filename, "%s", "fcct2.txt");
				break;
			case 3:
				sprintf(filename, "%s", "fcct3.txt");
				break;
			case 4:
				sprintf(filename, "%s", "fcct4.txt");
				break;
			case 5:
				sprintf(filename, "%s", "fcct5.txt");
				break;
			default: sprintf(filename, "%s", "fcct1.txt");
			}
		}

#ifdef DEBUG_PRINT1
		printf("Filename: %s\n", filename);
#endif

		fp = fopen(filename, "r");
		if (myid == 0)
			logfile = fopen("mylogfile.log", "a+");
		if (fp == NULL) {
			cout << "ERROR: Cant open file. Process " << myid << " of " << numprocs << " on " << processor_name << endl;
		}
		line_count = 0;
		while (!feof(fp)) {
			if (line_count == 0) {
				fscanf(fp, "%d", &r);
			}
			else if (line_count == 1) {
				fscanf(fp, "%d", &w);
			}
			else {
				fscanf(fp, "%d %d %d %d %d %d\n", &x1, &y1, &p1, &x2, &y2, &p2);
				if (x1 == -1 || y1 == -1 || x2 == -1 || y2 == -1)
					break;
				else {
					add_connection(my_connections + line_count - 2, x1, y1, p1, x2, y2, p2);
				}
			}
			line_count++;
		}
		fclose(fp);
		startwtime = MPI_Wtime();	// Get Starting time
	} // End Process 0 portion
	MPI_Bcast(&line_count, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&r, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&w, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&my_connections, 12100, MPI_BYTE, 0, MPI_COMM_WORLD);  // if it doesn't work change to 10000
	MPI_Barrier(MPI_COMM_WORLD);	// Synchronize all processes
#ifdef DEBUG_PRINT1
	printf("R: %d\nW: %d\nline_count: %d\n", r, w, line_count);
#endif
	point_count = line_count - 2;
	// Create grid - it will have (r+2)*2-1 rows and colums
	// This number comes from the fact that there are 2 extra rows/colums for IOs,
	// and we'll need to label both BLEs and routing resources on the grid

	grid_size = (r + 2) * 2 - 1;
#ifdef DEBUG_PRINT1
	printf("Process %d found Grid size %d, r: %d\n", myid, grid_size, r);
#endif

	// Fill the grid:
	// 99 		-> blocked off
	// 98 		-> BLE
	// 97 		-> Switchbox - assume routable
	// 96		-> Routing track
	// w 		-> Routing track - the value will tell us how many tracks are available
	for (k = 0; k < (r + 2) * 2 - 1; k++) {
		for (j = 0; j < (r + 2) * 2 - 1; j++) {
			if (k == 0 || k == grid_size - 1) {
				if (j % 2 == 0) {
					grid[j][k] = 98;
					grid_tracks[j][k] = 98;
				}
				else {
					grid[j][k] = 99;
					grid_tracks[j][k] = 99;
				}
			}
			else if (k % 2 == 0) {
				if (j % 2 == 0) {
					grid[j][k] = 98;
					grid_tracks[j][k] = 98;
				}
				else {
					grid[j][k] = 96;
					grid_tracks[j][k] = w;
				}
			}
			else {
				if (j == 0 || j == grid_size - 1) {
					grid[j][k] = 99;
					grid_tracks[j][k] = 99;
				}
				else if (j % 2 == 0) {
					grid[j][k] = 96;
					grid_tracks[j][k] = w;
				}
				else {
					grid[j][k] = 97;
					grid_tracks[j][k] = 97;
				}
			}
		}
	}


	// Block off corners
	grid[0][0] = 99;
	grid[0][grid_size - 1] = 99;
	grid[grid_size - 1][0] = 99;
	grid[grid_size - 1][grid_size - 1] = 99;

	// Print out grid to make sure we're ok
#ifdef PRINT_GRIDS_INITIAL
	for (k = (r + 2) * 2 - 2; k >= 0; k--) {
		printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		for (j = 0; j < (r + 2) * 2 - 1; j++) {
			printf("| %2d ", grid[j][k]);
		}
		printf("|\n");
	}
	printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	for (k = (r + 2) * 2 - 2; k >= 0; k--) {
		printf("============================================================================\n");
		for (j = 0; j < (r + 2) * 2 - 1; j++) {
			printf("| %2d ", grid_tracks[j][k]);
		}
		printf("|\n");
	}
	printf("============================================================================\n");
#endif
	//Copy Grids -starting_grid is non-routed
	for (k = (r + 2) * 2 - 2; k >= 0; k--) {
		for (j = 0; j < (r + 2) * 2 - 1; j++) {
			starting_grid[j][k] = grid[j][k];
		}
	}
	//}	// End Process 0 portion

	// Send grid to all processes
	//MPI::COMM_WORLD.Bcast(&grid, 10000, MPI_INT, 0); 
	//MPI::COMM_WORLD.Bcast(&grid_tracks, 10000, MPI_INT, 0); 
	//MPI::COMM_WORLD.Bcast(&my_connections, 10000, MPI_INT, 0); 
	//MPI::COMM_WORLD.Bcast(&starting_grid, 10000, MPI_INT, 0); 
	//MPI::COMM_WORLD.Bcast(&point_count, 1, MPI_INT, 0); 
	//MPI::COMM_WORLD.Bcast(&r, 1, MPI_INT, 0); 

	// MAIN FOR LOOP
	for (conn = 0; conn < point_count; conn++) {
		if (conn % numprocs == myid) {
#ifdef DEBUG_PRINT1
			cout << "Conn: " << conn << " of " << point_count << " on " << myid << endl;
#endif
			//grid = starting_grid;
			for (k = (r + 2) * 2 - 2; k >= 0; k--) {
				for (j = 0; j < (r + 2) * 2 - 1; j++) {
					grid[j][k] = starting_grid[j][k];
				}
			}
			connection* s = my_connections + conn;
			// Convert coordinates to new grid
			s->x1 = s->x1 * 2;
			s->x2 = s->x2 * 2;
			s->y1 = s->y1 * 2;
			s->y2 = s->y2 * 2;

			// Check that start and enpoints are correct
			if ((grid[s->x1][s->y1] != 98) || (grid[s->x2][s->y2] != 98)) {
				printf("Startpoint: X: %d Y: %d\n", grid[s->x1][s->y1]);
				printf("Endpoint:   X: %d Y: %d\n", grid[s->x2][s->y2]);
				printf("Error, incorrect start or endpoint\n");
			}
			// Get start pin
			// Check if it's an IO port
			if (s->x1 == 0) {
				startX = s->x1 + 1;
				startY = s->y1;
			}
			else if (s->y1 == 0) {
				startX = s->x1;
				startY = s->y1 + 1;
			}
			else if (s->x1 == grid_size - 1) {
				startX = s->x1 - 1;
				startY = s->y1;
			}
			else if (s->y1 == grid_size - 1) {
				startX = s->x1;
				startY = s->y1 - 1;
			}
			else { // Move connection based on which pin we're connecting
				if (s->p1 == 0) {
					startX = s->x1;
					startY = s->y1 + 1;
				}
				else if (s->p1 == 1) {
					startX = s->x1 - 1;
					startY = s->y1;
				}
				else if (s->p1 == 2) {
					startX = s->x1;
					startY = s->y1 - 1;
				}
				else if (s->p1 == 3) {
					startX = s->x1 + 1;
					startY = s->y1;
				}
				else if (s->p1 == 4) {
					startX = s->x1 + 1;
					startY = s->y1;
				}
			}
			// Get end pin
			// Check if it's an IO port
			if (s->x2 == 0) {
				endX = s->x2 + 1;
				endY = s->y2;
			}
			else if (s->y2 == 0) {
				endX = s->x2;
				endY = s->y2 + 1;
			}
			else if (s->x2 == grid_size - 1) {
				endX = s->x2 - 1;
				endY = s->y2;
			}
			else if (s->y2 == grid_size - 1) {
				endX = s->x2;
				endY = s->y2 - 1;
			}
			else { // Move connection based on which pin we're connecting
				if (s->p2 == 0) {
					endX = s->x2;
					endY = s->y2 + 1;
				}
				else if (s->p2 == 1) {
					endX = s->x2 - 1;
					endY = s->y2;
				}
				else if (s->p2 == 2) {
					endX = s->x2;
					endY = s->y2 - 1;
				}
				else if (s->p2 == 3) {
					endX = s->x2 + 1;
					endY = s->y2;
				}
				else if (s->p2 == 1) {
					endX = s->x2 + 1;
					endY = s->y2;
				}
			}
			if (grid_tracks[startX][startY] > 0) {
				//grid_tracks[startX][startY]--;
				//printf("Routable %d\n", grid_tracks[startX][startY]);
			}
			else {
				// Can't route
				printf("Can't route this signal - all tracks used up %d\n", grid_tracks[startX][startY]);
				break;
			}
			printf("CONN %d :: Connecting loc %d, %d to loc %d, %d\n", conn, startX, startY, endX, endY);

			grid[startX][startY] = 0;	// Starting point
			level = 0;
			found = 0;
			while (found == 0) {
				for (k = 0; k < (r + 2) * 2 - 1; k++) {
					for (j = 0; j < (r + 2) * 2 - 1; j++) {
						//if(myid==1)
							//printf("j %d, k %d\n", j, k);
						if (grid[j][k] == level) {
							if (grid[j + 1][k] == 97) {
								grid[j + 1][k] = level + 1;
							}
							else if (grid[j + 1][k] == 96) {
								if (grid_tracks[j + 1][k] > 0) {
									grid[j + 1][k] = level + 1;
								}
							}
							if (grid[j][k + 1] == 97) {
								grid[j][k + 1] = level + 1;
							}
							else if (grid[j][k + 1] == 96) {
								if (grid_tracks[j][k + 1] > 0) {
									grid[j][k + 1] = level + 1;
								}
							}
							if (grid[j - 1][k] == 97) {
								grid[j - 1][k] = level + 1;
							}
							else if (grid[j - 1][k] == 96) {
								if (grid_tracks[j - 1][k] > 0) {
									grid[j - 1][k] = level + 1;
								}
							}
							if (grid[j][k - 1] == 97) {
								grid[j][k - 1] = level + 1;
							}
							else if (grid[j][k - 1] == 96) {
								if (grid_tracks[j][k - 1] > 0) {
									grid[j][k - 1] = level + 1;
								}
							}
						}
					}
				}
				level++;
				if (grid[endX][endY] != 96)
					found = 1;
			}
			printf("\n");
			// Back-track trough connection
			found = 0;
			j = endX;
			k = endY;
			level = grid[endX][endY];
			grid[endX][endY] = grid[endX][endY] + 100;
			grid_tracks[endX][endY]--;
			tracks_used++;
			while (found == 0) {
				if (level == 1) {
					found = 1;
				}
				if (grid[j - 1][k] == level - 1) {
					j = j - 1;
				}
				else if (grid[j][k - 1] == level - 1) {
					k = k - 1;
				}
				else if (grid[j + 1][k] == level - 1) {
					j = j + 1;
				}
				else if (grid[j][k + 1] == level - 1) {
					k = k + 1;
				}
				grid[j][k] = grid[j][k] + 100;
				if (grid_tracks[j][k] != 97) {
					grid_tracks[j][k]--;
					tracks_used++;
				}
				level--;
			}
			//print_grid(grid_tracks, r);
			// Print out grid to make sure we're ok
#ifdef PRINT_GRIDS			
			for (k = (r + 2) * 2 - 2; k >= 0; k--) {
#ifdef TEXT_PRINT
				for (z = 0; z <= (r + 2) * 2 - 2; z++) {
					printf("-----");
				}
				printf("\n");
#endif
				for (j = 0; j < (r + 2) * 2 - 1; j++)
				{
					if (grid[j][k] >= 100) {
#ifdef TEXT_PRINT
						printf("|*%2d ", grid[j][k] - 100);
#endif
						sprintf(my_string, "%2d", grid[j][k] - 100);
#ifdef SHOW_TRACKS
						setcolor(YELLOW);
						if (k % 2 == 0) {
							my_setcolor(grid_tracks[j][k] % 8);
							fillrect(10 + (float)j * CELL_SIZE + 10 * (grid_tracks[j][k]), 10 + (float)k * CELL_SIZE, 10 + (float)(j)*CELL_SIZE + 10 * (grid_tracks[j][k]) + 4, 10 + (float)(k + 1) * CELL_SIZE);
						}
						else if (j % 2 == 0) {
							my_setcolor(grid_tracks[j][k] % 8);
							fillrect(10 + (float)j * CELL_SIZE, 10 + (float)k * CELL_SIZE + 10 * (grid_tracks[j][k]), 10 + (float)(j + 1) * CELL_SIZE, 10 + (float)(k)*CELL_SIZE + 10 * (grid_tracks[j][k]) + 4);
						}
						else {
							// Switchbox
							setlinestyle(DASHED);
							drawline(10 + (float)(j + 1) * CELL_SIZE, 10 + (float)(k)*CELL_SIZE, 10 + (float)(j)*CELL_SIZE, 10 + (float)(k)*CELL_SIZE + CELL_SIZE);
							drawline(10 + (float)j * CELL_SIZE, 10 + (float)k * CELL_SIZE, 10 + (float)(j)*CELL_SIZE + CELL_SIZE, 10 + (float)(k + 1) * CELL_SIZE);
							setlinestyle(SOLID);
						}
						setcolor(BLACK);
#endif	
#ifdef SHOW_PATHS				
						my_setcolor(conn % 8);
						if (k % 2 == 0) {
							drawline(10 + (float)(j)*CELL_SIZE + 10 * conn + 10, 10 + (float)(k)*CELL_SIZE, 10 + (float)(j)*CELL_SIZE + 10 * conn + 10, 10 + (float)(k + 1) * CELL_SIZE);
							prevJ = j; // vertical line
							prevK = k;
						}
						else if (j % 2 == 0) {
							drawline(10 + (float)(j)*CELL_SIZE, 10 + (float)(k)*CELL_SIZE + 10 * conn + 10, 10 + (float)(j + 1) * CELL_SIZE, 10 + (float)(k)*CELL_SIZE + 10 * conn + 10);
							prevJ = j; // horizontal line
							prevK = k;
						}
						else {
							// Switchbox
							printf("\nPrevJ %d PrevK %d\nj: %d, k: %d\n", prevJ, prevK, j, k);
							if (prevJ > j) {	// previous block above
								drawline(10 + (float)(j)*CELL_SIZE, 10 + (float)(k)*CELL_SIZE + 10 * conn + 10, 10 + (float)(j + 1) * CELL_SIZE, 10 + (float)(k)*CELL_SIZE + 10 * conn + 10);
							}
							else if (prevJ < j) {	// previous block below
								drawline(10 + (float)(j)*CELL_SIZE, 10 + (float)(k)*CELL_SIZE + 10 * conn + 10, 10 + (float)(j)*CELL_SIZE + 10 * conn + 10, 10 + (float)(k)*CELL_SIZE + 10 * conn + 10);
								drawline(10 + (float)(j)*CELL_SIZE + 10 * conn + 10, 10 + (float)(k)*CELL_SIZE, 10 + (float)(j)*CELL_SIZE + 10 * conn + 10, 10 + (float)(k)*CELL_SIZE + 10 * conn + 10);
							}
							else if (prevK < k) {	// previous block to the left
								drawline(10 + (float)(j)*CELL_SIZE, 10 + (float)(k)*CELL_SIZE + 10 * conn + 10, 10 + (float)(j + 1) * CELL_SIZE, 10 + (float)(k)*CELL_SIZE + 10 * conn + 10);
							}
							else if (prevK > k) {	// previous block to the right
								drawline(10 + (float)(j)*CELL_SIZE + 10 * conn + 10, 10 + (float)(k)*CELL_SIZE, 10 + (float)(j)*CELL_SIZE + 10 * conn + 10, 10 + (float)(k + 1) * CELL_SIZE);
							}
						}
#endif
						//drawtext (10+(float)j*CELL_SIZE+CELL_SIZE/2,10+(float)k*CELL_SIZE+CELL_SIZE/2,my_string,500.);
					}
					else {
#ifdef TEXT_PRINT
						printf("| %2d ", grid[j][k]);
#endif
					}
				}
#ifdef TEXT_PRINT
				printf("|\n");
#endif
			}
#ifdef TEXT_PRINT
			for (z = 0; z <= (r + 2) * 2 - 2; z++) {
				printf("-----");
			}
			printf("\n");
#endif
			// Tracks
#ifdef TEXT_PRINT
			for (k = (r + 2) * 2 - 2; k >= 0; k--) {
				for (z = 0; z <= (r + 2) * 2 - 2; z++) {
					printf("+++++");
				}
				printf("\n");
				for (j = 0; j < (r + 2) * 2 - 1; j++)
				{
					if (grid_tracks[j][k] >= 100) {
						printf("|*%2d ", grid_tracks[j][k] - 100);
					}
					else {
						printf("| %2d ", grid_tracks[j][k]);
					}
				}
				printf("|\n");
			}
			for (z = 0; z <= (r + 2) * 2 - 2; z++) {
				printf("+++++");
			}
			printf("\n");
#endif
#endif
		}
	}

	MPI_Reduce(&tracks_used, &total_tracks_used, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
	if (myid == 0)
		printf("Tracks Used: %d\n", total_tracks_used);

	n = 10000;			/* default # of rectangles */

	MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

	h = 1.0 / (double)n;
	sum = 0.0;

	MPI_Reduce(&mypi, &pi, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

	if (myid == 0) {
		// Get Runtime and put it in the logfile
		endwtime = MPI_Wtime();
		cout << "wall clock time = " << endwtime - startwtime << endl;
		fprintf(logfile, "File: %s\tTime: %.8f\tNum Procs: %d\n", filename, endwtime - startwtime, numprocs);
		fclose(logfile);
	}

	endMe(myid, processor_name);
	//	delete []grid;
	return 0;
}

void endMe(int myid, char* processor_name) {
	MPI_Barrier(MPI_COMM_WORLD);
	cout << "EXITING Process #" << myid << " on " << processor_name << endl;
	MPI_Finalize();
}
connection* add_connection(connection* s, int x1, int y1, int p1, int x2, int y2, int p2)
{
	s->x1 = x1;
	s->y1 = y1;
	s->p1 = p1;
	s->x2 = x2;
	s->y2 = y2;
	s->p2 = p2;
	return s;
}

void print_connection(connection* s, int x1, int y1, int p1, int x2, int y2, int p2)
{
	printf("X1: %d\n", s->x1);
	printf("Y1: %d\n", s->y1);
	printf("P1: %d\n", s->p1);
	printf("X2: %d\n", s->x2);
	printf("Y2: %d\n", s->y2);
	printf("P2: %d\n", s->p2);
}

void print_grid(int grid[][110], int r) {
	int k, j;
	for (k = 0; k < (r + 2) * 2 - 1; k++)
	{
		for (j = 0; j < (r + 2) * 2 - 1; j++)
		{
			printf("loc %d ", grid[j][k]);
		}
		printf("\n");
	}
}
