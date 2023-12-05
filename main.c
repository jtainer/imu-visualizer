// 
// IMU Visualizer
// Reads IMU tracking data over virtual serial port
// Displays position and orientation using a virtual object
//
// Copyright (c) 2023 Jonathan Tainer. Subject to the BSD 2-Clause License.
//

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <raylib.h>
#include <raymath.h>
#include <math.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define RAYLIB_5_0

volatile int modem_thread_stop = 0;
int modem_fd = 0;

volatile Vector4 orientation = { 0 };

// Read from serial port in a separate thread to avoid blocking draw loop
void* modem_thread(void* arg);

void* render_thread(void* arg);

int main(int argc, char** argv) {
	// Serial port setup
	if (argc < 2) {
		printf("No serial port indicated\n");
		return 0;
	}

	const char* modem_dev = argv[1];
	modem_fd = open(modem_dev, O_RDWR | O_NOCTTY);
	if (modem_fd < 0) {
		printf("Failed to open modem device: %s\n", modem_dev);
		return 1;
	}

	struct termios oldtio = { 0 }, newtio = { 0 };
	tcgetattr(modem_fd, &oldtio); // Save current serial port settings
	newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR | ICRNL;
	newtio.c_oflag = 0;
	newtio.c_lflag = ICANON;
	newtio.c_cc[VINTR]	= 0;	/* Ctrl-c */ 
	newtio.c_cc[VQUIT]	= 0;	/* Ctrl-\ */
	newtio.c_cc[VERASE]	= 0;	/* del */
	newtio.c_cc[VKILL]	= 0;	/* @ */
	newtio.c_cc[VEOF]	= 4;	/* Ctrl-d */
	newtio.c_cc[VTIME]	= 0;	/* inter-character timer unused */
	newtio.c_cc[VMIN]	= 1;	/* blocking read until 1 character arrives */
	newtio.c_cc[VSWTC]	= 0;	/* '\0' */
	newtio.c_cc[VSTART]	= 0;	/* Ctrl-q */ 
	newtio.c_cc[VSTOP]	= 0;	/* Ctrl-s */
	newtio.c_cc[VSUSP]	= 0;	/* Ctrl-z */
	newtio.c_cc[VEOL]	= 0;	/* '\0' */
	newtio.c_cc[VREPRINT]	= 0;	/* Ctrl-r */
	newtio.c_cc[VDISCARD]	= 0;	/* Ctrl-u */
	newtio.c_cc[VWERASE]	= 0;	/* Ctrl-w */
	newtio.c_cc[VLNEXT]	= 0;	/* Ctrl-v */
	newtio.c_cc[VEOL2]	= 0;	/* '\0' */

	// Clear modem line and activate new port settings
	tcflush(modem_fd, TCIFLUSH);
	tcsetattr(modem_fd, TCSANOW, &newtio);

	modem_thread_stop = false;
	pthread_t thread_handle = { 0 };
	pthread_create(&thread_handle, NULL, modem_thread, NULL);

	render_thread(NULL);

	modem_thread_stop = true;
	pthread_join(thread_handle, NULL);

	// Restore old port settings
	tcsetattr(modem_fd, TCSANOW, &oldtio);
	close(modem_fd);

	return 0;
}

void* modem_thread(void* arg) {
	const int buflen = 1024;
	char buf[buflen];
	int res = 0;
	while (!modem_thread_stop) {
		res = read(modem_fd, buf, buflen);
		buf[res] = 0;
		float w = 0, x = 0, y = 0, z = 0;
		int conv = sscanf(buf, "w = %f x = %f y = %f z = %f\n", &w, &x, &y, &z);
		if (conv == 4) {
			orientation = (Vector4) { x, y, z, w };
		}
	}
	return NULL;
}

void* render_thread(void* arg) {
	SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN | FLAG_VSYNC_HINT);
	InitWindow(2560, 1440, "IMU Visualizer");
	SetTargetFPS(120);

	#ifdef RAYLIB_5_0
	SetTextLineSpacing(40);
	#endif
	
	Camera camera = { 0 };
	camera.projection = CAMERA_PERSPECTIVE;
	camera.fovy = 90.f;
	camera.position = (Vector3) { -3.f, 3.f, -0.f };
	camera.target = (Vector3) { 0.f, 0.f, 0.f };
	camera.up = (Vector3) { 0.f, 1.f, 0.f };

	Model cube_model = LoadModelFromMesh(GenMeshCube(1.f, 1.f, 1.f));

	while (!WindowShouldClose()) {
		
		// Model parameters
		Vector3 pos = { 0.f, 1.f, 0.f };
		float scale = 1.f;

		// Swap axes of coordinate system and convert quaternion to rotation matrix
		Vector4 rot = { orientation.x, orientation.z, orientation.y, orientation.w };
		cube_model.transform = QuaternionToMatrix(rot);

		BeginDrawing();
		ClearBackground(BLACK);
		BeginMode3D(camera);
		DrawGrid(10,1);
		DrawModel(cube_model, pos, scale, RED);
		DrawModelWires(cube_model, pos, scale, BLACK);
		EndMode3D();
		EndDrawing();
	}
	UnloadModel(cube_model);
	CloseWindow();
	return NULL;
}
