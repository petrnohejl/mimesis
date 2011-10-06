// ============================================================================
//	Includes
// ============================================================================

#ifdef _WIN32
#  include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#  include <GLUT/glut.h>
#else
#  include <GL/glut.h>
#endif

#include <AR/config.h>
#include <AR/gsub.h>
#include <AR/video.h>
#include <AR/param.h>			// arParamDisp()
#include <AR/ar.h>
#include <AR/arMulti.h>
#include <AR/gsub_lite.h>
#include <AR/arvrml.h>

#include "object.h"

// ============================================================================
//	Constants
// ============================================================================

#define VIEW_SCALEFACTOR		0.025		// 1.0 ARToolKit unit becomes 0.025 of my OpenGL units.
#define VIEW_SCALEFACTOR_1		1.0			// 1.0 ARToolKit unit becomes 1.0 of my OpenGL units.
#define VIEW_SCALEFACTOR_4		4.0			// 1.0 ARToolKit unit becomes 4.0 of my OpenGL units.
#define VIEW_DISTANCE_MIN		4.0			// Objects closer to the camera than this will not be displayed.
#define VIEW_DISTANCE_MAX		32000.0		// Objects further away from the camera than this will not be displayed.


// ============================================================================
//	Global variables
// ============================================================================

// Preferences.
static int prefWindowed = TRUE;
static int prefWidth = 640;										// Fullscreen mode width.
static int prefHeight = 480;									// Fullscreen mode height.
static int prefDepth = 32;										// Fullscreen mode bit depth.
static int prefRefresh = 0;										// Fullscreen mode refresh rate. Set to 0 to use default rate.
static char prefCaption[64] = "Mantis Augmented Reality - Designblok 2010";		// Window caption

// Image acquisition.
static ARUint8		*gARTImage = NULL;

// Marker detection.
static int			gARTThreshhold = 100;
static long			gCallCountMarkerDetect = 0;

// Transformation matrix retrieval.
static int			gPatt_found = FALSE;	// At least one marker.
static int			gPatt_found_multi = FALSE;	// At least one multi marker.

// Drawing.
static ARParam		gARTCparam;
static ARGL_CONTEXT_SETTINGS_REF gArglSettings = NULL;

// Object Data.
static ObjectData_T			*gObjectData;
static int					gObjectDataCount;
static ARMultiMarkerInfoT	*gMultiMarkerConfig;
static double				gErr;

// Show current object model
static int gObjectModel = 0;

// Switchers
static int gDebugText;
static int gDrawAlways;
static int gFullscreen;

// Position
static float gPosX;
static float gPosY;
static float gPosZ;

// Rotation
static float gRotX;
static float gRotY;
static float gRotZ;


// ============================================================================
//	Declarations
// ============================================================================

void printString( char *string, double position );
static void Reshape(int w, int h);
static void draw( double trans1[3][4], double trans2[3][4], int mode );


// ============================================================================
//	Functions
// ============================================================================

static int setupCamera(const char *cparam_name, char *vconf, ARParam *cparam)
{	
    ARParam			wparam;
	int				xsize, ysize;

    // Open the video path.
    if (arVideoOpen(vconf) < 0) {
    	fprintf(stderr, "setupCamera(): Unable to open connection to camera.\n");
    	return (FALSE);
	}
	
    // Find the size of the window.
    if (arVideoInqSize(&xsize, &ysize) < 0) return (FALSE);
    fprintf(stdout, "Camera image size (x,y) = (%d,%d)\n", xsize, ysize);
	
	// Load the camera parameters, resize for the window and init.
    if (arParamLoad(cparam_name, 1, &wparam) < 0) {
		fprintf(stderr, "setupCamera(): Error loading parameter file %s for camera.\n", cparam_name);
        return (FALSE);
    }
    arParamChangeSize(&wparam, xsize, ysize, cparam);
    fprintf(stdout, "*** Camera Parameter ***\n");
    arParamDisp(cparam);

    arInitCparam(cparam);

	if (arVideoCapStart() != 0) {
    	fprintf(stderr, "setupCamera(): Unable to begin camera data capture.\n");
		return (FALSE);		
	}
	
	return (TRUE);
}

static int setupMarkersObjects(char *objectDataFilename, char *objectDataFilenameMulti)
{	
	// Load in the object data - trained markers and associated bitmap files.
    if ((gObjectData = read_VRMLdata(objectDataFilename, &gObjectDataCount)) == NULL) {
        fprintf(stderr, "setupMarkersObjects(): read_VRMLdata returned error !!\n");
        return (FALSE);
    }
    printf("Object count = %d\n", gObjectDataCount);

	if((gMultiMarkerConfig = arMultiReadConfigFile(objectDataFilenameMulti)) == NULL) {
        fprintf(stderr, "setupMarkersObjects(): arMultiReadConfigFile returned error !!\n");
		return (FALSE);
    }
	
	return (TRUE);
}

// Report state of ARToolKit global variables arFittingMode,
// arImageProcMode, arglDrawMode, arTemplateMatchingMode, arMatchingPCAMode.
static void debugReportMode(void)
{
	if(arFittingMode == AR_FITTING_TO_INPUT ) {
		fprintf(stderr, "FittingMode (Z): INPUT IMAGE\n");
	} else {
		fprintf(stderr, "FittingMode (Z): COMPENSATED IMAGE\n");
	}
	
	if( arImageProcMode == AR_IMAGE_PROC_IN_FULL ) {
		fprintf(stderr, "ProcMode (X)   : FULL IMAGE\n");
	} else {
		fprintf(stderr, "ProcMode (X)   : HALF IMAGE\n");
	}
	
	if (arglDrawModeGet(gArglSettings) == AR_DRAW_BY_GL_DRAW_PIXELS) {
		fprintf(stderr, "DrawMode (C)   : GL_DRAW_PIXELS\n");
	} else if (arglTexmapModeGet(gArglSettings) == AR_DRAW_TEXTURE_FULL_IMAGE) {
		fprintf(stderr, "DrawMode (C)   : TEXTURE MAPPING (FULL RESOLUTION)\n");
	} else {
		fprintf(stderr, "DrawMode (C)   : TEXTURE MAPPING (HALF RESOLUTION)\n");
	}
		
	if( arTemplateMatchingMode == AR_TEMPLATE_MATCHING_COLOR ) {
		fprintf(stderr, "TemplateMatchingMode (M)   : Color Template\n");
	} else {
		fprintf(stderr, "TemplateMatchingMode (M)   : BW Template\n");
	}
	
	if( arMatchingPCAMode == AR_MATCHING_WITHOUT_PCA ) {
		fprintf(stderr, "MatchingPCAMode (P)   : Without PCA\n");
	} else {
		fprintf(stderr, "MatchingPCAMode (P)   : With PCA\n");
	}
}

static void Quit(void)
{
	arglCleanup(gArglSettings);
	arVideoCapStop();
	arVideoClose();
#ifdef _WIN32
	CoUninitialize();
#endif
	exit(0);
}

static void Keyboard(unsigned char key, int x, int y)
{
	int mode;
	switch (key) {
		case 0x1B:						// Quit.
		case 'Q':
		case 'q':
			Quit();
			break;
		case 'U':
		case 'u':
			gPosX += 10;
			printf("Transform X+\n");
			printf("Position: %f %f %f\n", gPosX, gPosY, gPosZ);
			break;
		case 'I':
		case 'i':
			gPosY += 10;
			printf("Transform Y+\n");
			printf("Position: %f %f %f\n", gPosX, gPosY, gPosZ);
			break;
		case 'O':
		case 'o':
			gPosZ += 10;
			printf("Transform Z+\n");
			printf("Position: %f %f %f\n", gPosX, gPosY, gPosZ);
			break;
		case 'J':
		case 'j':
			gPosX -= 10;
			printf("Transform X-\n");
			printf("Position: %f %f %f\n", gPosX, gPosY, gPosZ);
			break;
		case 'K':
		case 'k':
			gPosY -= 10;
			printf("Transform Y-\n");
			printf("Position: %f %f %f\n", gPosX, gPosY, gPosZ);
			break;
		case 'L':
		case 'l':
			gPosZ -= 10;
			printf("Transform Z-\n");
			printf("Position: %f %f %f\n", gPosX, gPosY, gPosZ);
			break;
		case '1':
		case '+':
			gRotX += 2.5;
			printf("Rotate X+\n");
			printf("Rotation: %f %f %f\n", gRotX, gRotY, gRotZ);
			break;
		case '2':
		case 'ì':
			gRotY += 2.5;
			printf("Rotate Y+\n");
			printf("Rotation: %f %f %f\n", gRotX, gRotY, gRotZ);
			break;
		case '3':
		case 'š':
			gRotZ += 2.5;
			printf("Rotate Z+\n");
			printf("Rotation: %f %f %f\n", gRotX, gRotY, gRotZ);
			break;
		case '4':
		case 'è':
			gRotX -= 2.5;
			printf("Rotate X-\n");
			printf("Rotation: %f %f %f\n", gRotX, gRotY, gRotZ);
			break;
		case '5':
		case 'ø':
			gRotY -= 2.5;
			printf("Rotate Y-\n");
			printf("Rotation: %f %f %f\n", gRotX, gRotY, gRotZ);
			break;
		case '6':
		case 'ž':
			gRotZ -= 2.5;
			printf("Rotate Z-\n");
			printf("Rotation: %f %f %f\n", gRotX, gRotY, gRotZ);
			break;
		case 'X':
		case 'x':
			gObjectModel = (gObjectModel + 1) % gObjectDataCount;
			printf("Switch 3D model: %d / %d\n", gObjectModel, gObjectDataCount);
			break;
		case 'F':
		case 'f':
			gFullscreen = !gFullscreen;
			if(gFullscreen) 
			{
				glutFullScreen();
			}
			else 
			{
				glutReshapeWindow(prefWidth, prefHeight);
				glutPositionWindow(100,100);
			}
			printf("Fullscreen: %d\n", gFullscreen);
			break;
		case 'C':
		case 'c':
			mode = arglDrawModeGet(gArglSettings);
			if (mode == AR_DRAW_BY_GL_DRAW_PIXELS) {
				arglDrawModeSet(gArglSettings, AR_DRAW_BY_TEXTURE_MAPPING);
				arglTexmapModeSet(gArglSettings, AR_DRAW_TEXTURE_FULL_IMAGE);
			} else {
				mode = arglTexmapModeGet(gArglSettings);
				if (mode == AR_DRAW_TEXTURE_FULL_IMAGE)	arglTexmapModeSet(gArglSettings, AR_DRAW_TEXTURE_HALF_IMAGE);
				else arglDrawModeSet(gArglSettings, AR_DRAW_BY_GL_DRAW_PIXELS);
			}
			fprintf(stderr, "--------------------------------------\n");
			fprintf(stderr, "*** Camera - %f (frame/sec)\n", (double)gCallCountMarkerDetect/arUtilTimer());
			gCallCountMarkerDetect = 0;
			arUtilTimerReset();
			debugReportMode();
			fprintf(stderr, "--------------------------------------\n");
			break;
		case 'D':
		case 'd':
			arDebug = 1 - arDebug;
			if( arDebug == 0 ) {
				glClearColor( 0.0, 0.0, 0.0, 0.0 );
				glClear(GL_COLOR_BUFFER_BIT);
				argSwapBuffers();
				glClear(GL_COLOR_BUFFER_BIT);
				argSwapBuffers();
			}
			printf("Debug mode displaying threshold: %d\n", arDebug);
			break;
		case 'T':
		case 't':
			gDebugText = !gDebugText;
			printf("Debug text output: %d\n", gDebugText);
			break;
		case 'A':
		case 'a':
			gDrawAlways = !gDrawAlways;
			printf("Draw 3D models always including pattern off: %d\n", gDrawAlways);
			break;
		case 'W':
		case 'w':
			gARTThreshhold += 5;
			if(gARTThreshhold>255) gARTThreshhold=255;
			printf("Increasing threshold: %d\n", gARTThreshhold);
			break;
		case 'S':
		case 's':
			gARTThreshhold -= 5;
			if(gARTThreshhold<0) gARTThreshhold=0;
			printf("Decreasing threshold: %d\n", gARTThreshhold);
			break;
		case '?':
		case 'H':
		case 'h':
			printf("--------------------------------------\n");
			printf("Mantis Augmented Reality for Designblok 2010\n\n");
			printf("Copyright (c)2010 Petr Nohejl, www.jestrab.net\n\n");
			printf("Usage:\n");
			printf("   q or [esc]    Quit program\n");
			printf("   x             Switch 3D model\n");
			printf("   f             Change fullscreen mode\n");
			printf("   c             Change draw mode and texmap mode\n");
			printf("   d             Show debug mode displaying threshold\n");
			printf("   t             Show debug text output\n");
			printf("   a             Draw 3D models always including pattern off\n");
			printf("   w             Increase threshold\n");
			printf("   s             Decrease threshold\n");
			printf("   u i o         Increase position in X Y Z coordinates\n");
			printf("   j k l         Decrease position in X Y Z coordinates\n");
			printf("   1 2 3         Increase rotation in X Y Z coordinates\n");
			printf("   4 5 6         Decrease rotation in X Y Z coordinates\n");
			printf("   ? or h        Show this help\n");
			printf("\nAdditionally, the ARVideo library supplied the following help text:\n");
			arVideoDispOption();
			printf("--------------------------------------\n");
			break;
		default:
			break;
	}
}

static void Idle(void)
{
	static int ms_prev;
	int ms;
	float s_elapsed;
	ARUint8 *image;

	ARMarkerInfo    *marker_info;					// Pointer to array holding the details of detected markers.
    int             marker_num;						// Count of number of markers detected.
    int             i, j, k;
	
	// Find out how long since Idle() last ran.
	ms = glutGet(GLUT_ELAPSED_TIME);
	s_elapsed = (float)(ms - ms_prev) * 0.001;
	if (s_elapsed < 0.01f) return; // Don't update more often than 100 Hz.
	ms_prev = ms;
	
	// Update drawing.
	arVrmlTimerUpdate();
	
	// Grab a video frame.
	if ((image = arVideoGetImage()) != NULL) {
		gARTImage = image;	// Save the fetched image.
		gPatt_found = FALSE;	// Invalidate any previous detected markers.
		gPatt_found_multi = FALSE;	// Invalidate any previous detected multi markers.

		gCallCountMarkerDetect++; // Increment ARToolKit FPS counter.
		
		// Detect the markers in the video frame.
		if (arDetectMarker(gARTImage, gARTThreshhold, &marker_info, &marker_num) < 0) {
			exit(-1);
		}


		// Check for object visibility.
		for (i = 0; i < gObjectDataCount; i++) {
			// Check through the marker_info array for highest confidence
			// visible marker matching our object's pattern.
			k = -1;
			for (j = 0; j < marker_num; j++) {
				if (marker_info[j].id == gObjectData[i].id) {
					if( k == -1 ) k = j; // First marker detected.
					else if (marker_info[k].cf < marker_info[j].cf) k = j; // Higher confidence marker detected.
				}
			}

			
			if (k != -1) {
				// Get the transformation between the marker and the real camera.
				//fprintf(stderr, "Saw object %d.\n", i);
				if (gObjectData[i].visible == 0) {
					arGetTransMat(&marker_info[k], gObjectData[i].marker_center, gObjectData[i].marker_width, gObjectData[i].trans);
				} else {
					arGetTransMatCont(&marker_info[k], gObjectData[i].trans, gObjectData[i].marker_center, gObjectData[i].marker_width, gObjectData[i].trans);
				}
				gObjectData[i].visible = 1;
				//printf("Vidim te! %d\n", i);
				gPatt_found = TRUE;
			} 
			else {
				gObjectData[i].visible = 0;
				//printf("Nevidim te! %d\n", i);
			}
		}
	

		// Tell GLUT to update the display.
		glutPostRedisplay();


		// Compute camera position in function of the multi-marker patterns (based on detected markers) 
		if( (gErr = arMultiGetTransMat(marker_info, marker_num, gMultiMarkerConfig)) < 0 ) {

		}
		else
		{
			if(gMultiMarkerConfig->marker_num > 0) gPatt_found_multi = TRUE;
		}

	}
}

//
//	This function is called on events when the visibility of the
//	GLUT window changes (including when it first becomes visible).
//
static void Visibility(int visible)
{
	if (visible == GLUT_VISIBLE) {
		glutIdleFunc(Idle);
	} else {
		glutIdleFunc(NULL);
	}
}

//
//	This function is called when the
//	GLUT window is resized.
//
static void Reshape(int w, int h)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, (GLsizei) w, (GLsizei) h);
	
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// Call through to anyone else who needs to know about window sizing here.
}


//
// This function is called when the window needs redrawing.
//
static void Display(void)
{
    GLdouble p[16];
	GLdouble m[16];

	// Lights
    GLfloat   light_position[]  = {gPosX, gPosY, gPosZ, 0.0};
    GLfloat   ambi[]            = {0.1, 0.1, 0.1, 0.1};
    GLfloat   lightZeroColor[]  = {0.9, 0.9, 0.9, 0.1};

	// Select correct buffer for this context.
	glDrawBuffer(GL_BACK);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the buffers for new frame.

	// Display video frame
	if( !arDebug ) {
        arglDispImage(gARTImage, &gARTCparam, 1.0, gArglSettings);	// zoom = 1.0.
    }
	// Threshold debug video frame
    else {
		arglDispImage(gARTImage, &gARTCparam, 1.0, gArglSettings);
		arglDispImage(arImage, &gARTCparam, 1.0, gArglSettings);
    }

	arVideoCapNext();
	gARTImage = NULL; // Image data is no longer valid after calling arVideoCapNext().

	// Projection transformation.
	arglCameraFrustumRH(&gARTCparam, VIEW_DISTANCE_MIN, VIEW_DISTANCE_MAX, p);
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixd(p);
	glMatrixMode(GL_MODELVIEW);
	
	// Viewing transformation.
	glLoadIdentity();

	// Lighting and geometry that moves with the camera should go here.
	// (I.e. must be specified before viewing transformations.)
	//none


	/*
	// Draw VRML model for multi pattern
	if(gDrawAlways || gPatt_found_multi)
	{
		arglCameraViewRH(gMultiMarkerConfig->trans, m, VIEW_SCALEFACTOR_4);
		glLoadMatrixd(m);
		arVrmlDraw(gObjectData[ gObjectModel ].vrml_id);
	}
	*/


	glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambi);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightZeroColor);


	
	// Pattern priority
	if(arDebug) printf("VISIBILITY: %d %d %d %d %d\n", gObjectData[0].visible, gObjectData[1].visible, gObjectData[2].visible, gObjectData[3].visible, gObjectData[4].visible);
	
	if(gObjectData[0].visible) gObjectModel = 0;
	else if(gObjectData[1].visible) gObjectModel = 1;
	else if(gObjectData[2].visible) gObjectModel = 2;
	else if(gObjectData[3].visible) gObjectModel = 3;
	else if(gObjectData[4].visible) gObjectModel = 4;
	


	
	// Draw VRML model for single pattern
	if(gDrawAlways || gPatt_found)
	{
		arglCameraViewRH(gObjectData[ gObjectModel ].trans, m, VIEW_SCALEFACTOR_4);
		glLoadMatrixd(m);

		
		switch(gObjectModel)
		{
			case 0:
				// position of C
				glTranslatef( 210, -680, 4110 );
				glRotatef(177.0, 1.0, 0.0, 0.0);
				glRotatef(180.0, 0.0, 1.0, 0.0);
				glRotatef(0.0, 0.0, 0.0, 1.0);
				break;
			case 1:
				// position of sample1
				glTranslatef( 0, 2580, 2100 );
				glRotatef(65, 1.0, 0.0, 0.0);
				glRotatef(180, 0.0, 1.0, 0.0);
				glRotatef(0, 0.0, 0.0, 1.0);
				break;
			case 2:
				// position of F
				glTranslatef( 0, 4880, 2020 );
				glRotatef(117.5, 1.0, 0.0, 0.0);
				glRotatef(180.0, 0.0, 1.0, 0.0);
				glRotatef(0.0, 0.0, 0.0, 1.0);
				break;
			case 3:
				// position of hiro
				glTranslatef( 920, 3160, 2470 );
				glRotatef(102.5, 1.0, 0.0, 0.0);
				glRotatef(192.5, 0.0, 1.0, 0.0);
				glRotatef(2.5, 0.0, 0.0, 1.0);
				break;
			case 4:
				// position of kanji
				glTranslatef( -920, 3290, 2620 );
				glRotatef(102.5, 1.0, 0.0, 0.0);
				glRotatef(165.0, 0.0, 1.0, 0.0);
				glRotatef(0.0, 0.0, 0.0, 1.0);
				break;
			case 5:
				// position of A
				glTranslatef( 2930, 3290, 2580 );
				glRotatef(120, 1.0, 0.0, 0.0);
				glRotatef(162.5, 0.0, 1.0, 0.0);
				glRotatef(17.5, 0.0, 0.0, 1.0);
				break;
			case 6:
				// position of sample2
				glTranslatef( -3110, -2780, 2810 );
				glRotatef(32.5, 1.0, 0.0, 0.0);
				glRotatef(307.5, 0.0, 1.0, 0.0);
				glRotatef(-797.5, 0.0, 0.0, 1.0);
				break;
			
		}
		

		/*
		// variable position 
		glTranslatef( gPosX, gPosY, gPosZ );
		glRotatef(gRotX, 1.0, 0.0, 0.0);
		glRotatef(gRotY, 0.0, 1.0, 0.0);
		glRotatef(gRotZ, 0.0, 0.0, 1.0);
		*/

		arVrmlDraw(gObjectData[ 0 ].vrml_id);
	}
	

	/*
	// All other lighting and geometry goes here.
	// Calculate the camera position for each object and draw it.
	for (int i = 0; i < gObjectDataCount; i++) {
		if ((gObjectData[i].visible != 0) && (gObjectData[i].vrml_id >= 0)) {
			//fprintf(stderr, "About to draw object %i\n", i);
			arglCameraViewRH(gObjectData[i].trans, m, VIEW_SCALEFACTOR_4);
			glLoadMatrixd(m);

			arVrmlDraw(gObjectData[i].vrml_id);
		}			
	}
	*/

	// Lights off
	glDisable( GL_LIGHT0 );
	glDisable( GL_LIGHTING );


	// Debug text info
	if (gPatt_found_multi) {
		char string[256];
		sprintf(string, "Multi [x: %3.1f] [y: %3.1f] [z: %3.1f] [err: %3.1f]", gMultiMarkerConfig->trans[0][3], gMultiMarkerConfig->trans[1][3], gMultiMarkerConfig->trans[2][3], gErr);
		printString(string, 0.73);
	}
	else
	{
		printString("No multi pattern detected", 0.73);
	}

	if (gPatt_found) {	
		char string[256];
		sprintf(string, "Single #%d [x: %3.1f] [y: %3.1f] [z: %3.1f]", gObjectModel, gObjectData[ gObjectModel ].trans[0][3],gObjectData[ gObjectModel ].trans[1][3],gObjectData[ gObjectModel ].trans[2][3]);
		printString(string, 0.83);
	}
	else
	{
		printString("No single pattern detected", 0.83);
	}

	glutSwapBuffers();
}



void printString( char *string, double position )
{
  int len;
  int i;

  if(!gDebugText) return;
	
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  // display the position data
  glTranslatef(-0.95, position, 0.0);

  // draw a white polygon
  glColor3f(0.1, 0.1, 0.1);
  glBegin(GL_POLYGON);
    glVertex2f(1.5, 0.1);
    glVertex2f(1.5, 0.0);
    glVertex2f(0.0, 0.0);
    glVertex2f(0.0, 0.1);
  glEnd();

  // draw text on the polygon
  glColor3f(0.0, 1.0, 0.0);
  glRasterPos2f(0.02, 0.035);

  len = strlen(string);
  
  for (i=0; i<len; i++) 
  {
      if(string[i] != '\n' ) {
          glutBitmapCharacter(GLUT_BITMAP_8_BY_13, string[i]);
      }
      else {
          glTranslatef(0.0, 0.0, 0.0);
          glRasterPos2i(0.0, 0.0);
      }
  }

  return;
}

int parseConfig(char *filename)
{
	/*
	printf("PARSE: %s\n", filename);

	char s[1024];

	FILE *fr;
	fr = fopen(filename, "r");

	if (!fr) {
		fprintf(stderr, "parseConfig(): Unable to open config file!\n");
		return 1;
	}

	//char var1[64];
	int var1;
	int var2;
	int var3;
	fscanf (fr, "%d\n", var1);
	fscanf (fr, "%d\n", var2);
	fscanf (fr, "%d\n", var3);

	fclose(fr);

	printf("PARSE DATA: %d\n", var1);
	printf("PARSE DATA: %d\n", var2);
	printf("PARSE DATA: %d\n", var3);

	*/
	return 0;
}




int main(int argc, char** argv)
{
	int i;
	char glutGamemode[32];
	const char *cparam_name = "Data/camera_para.dat";
#ifdef _WIN32
	char			*vconf = "Data\\WDM_camera_flipV.xml";
#else
	char			*vconf = "";
#endif
	char objectDataFilename[] = "Data/object_data_mantis";
	char objectDataFilenameMulti[] = "Data/multi/marker_mantis.dat";

	// Load config file
	//if(parseConfig("config.txt") == 1) return 1;
	gDebugText = 0;
	gDrawAlways = 0;
	gFullscreen = 0;

	// Init positions
	gPosX = 0;
	gPosY = 0;
	gPosZ = 0;
	gRotX = 0;
	gRotY = 0;
	gRotZ = 0;




	// ----------------------------------------------------------------------------
	// Library inits.
	//

	glutInit(&argc, argv);

	// ----------------------------------------------------------------------------
	// Hardware setup.
	//

	if (!setupCamera(cparam_name, vconf, &gARTCparam)) {
		fprintf(stderr, "main(): Unable to set up AR camera.\n");
		exit(-1);
	}
	
#ifdef _WIN32
	CoInitialize(NULL);
#endif

	// ----------------------------------------------------------------------------
	// Library setup.
	//

	// Set up GL context(s) for OpenGL to draw into.
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
	if (!prefWindowed) {
		if (prefRefresh) sprintf(glutGamemode, "%ix%i:%i@%i", prefWidth, prefHeight, prefDepth, prefRefresh);
		else sprintf(glutGamemode, "%ix%i:%i", prefWidth, prefHeight, prefDepth);
		glutGameModeString(glutGamemode);
		glutEnterGameMode();
	} else {
		glutInitWindowSize(gARTCparam.xsize, gARTCparam.ysize);
		glutCreateWindow(prefCaption);
	}

	// Setup argl library for current context.
	if ((gArglSettings = arglSetupForCurrentContext()) == NULL) {
		fprintf(stderr, "main(): arglSetupForCurrentContext() returned error.\n");
		exit(-1);
	}
	debugReportMode();
	arUtilTimerReset();

	if (!setupMarkersObjects(objectDataFilename, objectDataFilenameMulti)) {
		fprintf(stderr, "main(): Unable to set up AR objects and markers.\n");
		Quit();
	}
	
	// Test render all the VRML objects.
    fprintf(stdout, "Pre-rendering the VRML objects...");
	fflush(stdout);
    glEnable(GL_TEXTURE_2D);
    for (i = 0; i < gObjectDataCount; i++) {
		arVrmlDraw(gObjectData[i].vrml_id);
    }
    glDisable(GL_TEXTURE_2D);
	fprintf(stdout, " done\n");
	fprintf(stdout, "--------------------------------------\n");
	
	// Register GLUT event-handling callbacks.
	// NB: Idle() is registered by Visibility.
	glutDisplayFunc(Display);
	glutReshapeFunc(Reshape);
	glutVisibilityFunc(Visibility);
	glutKeyboardFunc(Keyboard);
	
	glutMainLoop();

	return (0);
}
