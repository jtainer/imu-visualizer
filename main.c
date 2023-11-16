// 
// IMU Visualizer
// Reads IMU tracking data over virtual serial port
// Displays position and orientation using a virtual object
//
// Copyright 2023 Jonathan Tainer.
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

volatile int modem_thread_stop = 0;
int modem_fd = 0;

volatile Vector2 orientation = { 0 };

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
		int x = 0, y = 0;
		int conv = sscanf(buf, "Ang.x = %d\t\tAng.y = %d", &x, &y);
		if (conv == 2) {
			orientation = (Vector2) { (float)x, (float)y };
		}
	}
	return NULL;
}

void* render_thread(void* arg) {
	SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN | FLAG_VSYNC_HINT);
	InitWindow(1920, 1080, "IMU Visualizer");
	SetTargetFPS(120);
	
	Camera camera = { 0 };
	camera.projection = CAMERA_PERSPECTIVE;
	camera.fovy = 90.f;
	camera.position = (Vector3) { -3.f, 3.f, -0.f };
	camera.target = (Vector3) { 0.f, 0.f, 0.f };
	camera.up = (Vector3) { 0.f, 1.f, 0.f };

	Model cube_model = LoadModelFromMesh(GenMeshCube(1.f, 1.f, 1.f));

	while (!WindowShouldClose()) {
		const float rad = 300.f;
		float x_ang = DEG2RAD * orientation.x;
		float y_ang = DEG2RAD * orientation.y;
		Vector2 x_begin = { 1*1920/4, 1080/2 };
		Vector2 y_begin = { 3*1920/4, 1080/2 };
		Vector2 x_end = x_begin;
		Vector2 y_end = y_begin;
		x_end.x += rad * cosf(-x_ang);
		x_end.y += rad * sinf(-x_ang);
		y_end.x += rad * cosf(-y_ang);
		y_end.y += rad * sinf(-y_ang);

		Vector3 rotation_axis = { -x_ang, 0.f, y_ang };
		float rotation_angle = RAD2DEG * Vector3Length(rotation_axis);
		rotation_axis = Vector3Normalize(rotation_axis);
		Vector3 pos = { 0.f, 1.f, 0.f };
		Vector3 scale = { 1.f, 1.f, 1.f };

		BeginDrawing();
		ClearBackground(BLACK);
		BeginMode3D(camera);
		DrawGrid(10,1);
		DrawModelWiresEx(cube_model, pos, rotation_axis, rotation_angle, scale, RED);
		EndMode3D();
		DrawText(TextFormat("x = %f\ny = %f", orientation.x, orientation.y), 20,20,40,GREEN);
//		DrawLineV(x_begin, x_end, WHITE);
//		DrawLineV(y_begin, y_end, WHITE);
		EndDrawing();
	}
	UnloadModel(cube_model);
	CloseWindow();
	return NULL;
}
