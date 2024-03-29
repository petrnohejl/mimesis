#ifdef _WIN32
#  include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef __APPLE__
#  include <GL/glut.h>
#else
#  include <GLUT/glut.h>
#endif
#include <AR/gsub.h>
#include <AR/param.h>
#include <AR/ar.h>
#include <AR/video.h>

#include "object.h"

#define COLLIDE_DIST 30000.0

/* Object Data */
char            *model_name = "Data/object_data_lighting";
ObjectData_T    *object;
int             objectnum;

int             xsize, ysize;
int				thresh = 100;
int             count = 0;

/* set up the video format globals */

#ifdef _WIN32
char			*vconf = "Data\\WDM_camera_flipV.xml";
#else
char			*vconf = "";
#endif

char           *cparam_name    = "Data/camera_para.dat";
ARParam         cparam;

static int gDrawRotate = FALSE;
static float gDrawRotateAngle = 0;			// For use in drawing.

static void   init(void);
static void   cleanup(void);
static void   keyEvent( unsigned char key, int x, int y);
static void   mainLoop(void);
static int	  draw( ObjectData_T *object, int objectnum );
static int    draw_object( int obj_id, double gl_para[16] );
static void   rotateAnim(float timeDelta);

int main(int argc, char **argv)
{
	//initialize applications
	glutInit(&argc, argv);
    init();
	
	arVideoCapStart();

	//start the main event loop
    argMainLoop( NULL, keyEvent, mainLoop );

	return 0;
}

static void   keyEvent( unsigned char key, int x, int y)   
{
	switch (key) {
		case 0x1B: // ESC
		case 'Q':
		case 'q':
			printf("*** %f (frame/sec)\n", (double)count/arUtilTimer());
			cleanup();
			exit(0);
			break;
		case ' ':
			gDrawRotate = !gDrawRotate;
			break;
		case 'D':
		case 'd':
			/*
			printf("*** %f (frame/sec)\n", (double)count/arUtilTimer());
			arDebug = 1 - arDebug;
			if( arDebug == 0 ) {
				glClearColor( 0.0, 0.0, 0.0, 0.0 );
				glClear(GL_COLOR_BUFFER_BIT);
				argSwapBuffers();
				glClear(GL_COLOR_BUFFER_BIT);
				argSwapBuffers();
			}
			count = 0;
			*/
			break;
		case 'W':
		case 'w':
			thresh += 5;
			if(thresh>255) thresh=255;
			printf("Increasing threshold: %d\n", thresh);
			break;
		case 'S':
		case 's':
			thresh -= 5;
			if(thresh<0) thresh=0;
			printf("Decreasing threshold: %d\n", thresh);
			break;
		default:
			break;
	}
}

/* main loop */
static void mainLoop(void)
{
    ARUint8         *dataPtr;
    ARMarkerInfo    *marker_info;
    int             marker_num;
    int             i,j,k;
	
	static int ms_prev;
	int ms;
	float s_elapsed;

	// Find out how long since Idle() last ran.
	ms = glutGet(GLUT_ELAPSED_TIME);
	s_elapsed = (float)(ms - ms_prev) * 0.001;
	if (s_elapsed < 0.01f) return; // Don't update more often than 100 Hz.
	ms_prev = ms;

	// Update drawing.
	rotateAnim(s_elapsed);

    /* grab a video frame */
    if( (dataPtr = (ARUint8 *)arVideoGetImage()) == NULL ) {
        arUtilSleep(2);
        return;
    }
	
    if( count == 0 ) arUtilTimerReset();  
    count++;


	/*draw the video*/
    argDrawMode2D();
	argDispImage( dataPtr, 0, 0 );	


	glLineWidth(2.0);

	/* detect the markers in the video frame */ 
	if(arDetectMarker(dataPtr, thresh, 
		&marker_info, &marker_num) < 0 ) {
		cleanup(); 
		exit(0);
	}
	for( i = 0; i < marker_num; i++ ) {
		//glColor3f( 1.0, 0.0, 0.0 );
		//argDrawSquare(marker_info[i].vertex,0,0);
	}

	/* check for known patterns */
    for( i = 0; i < objectnum; i++ ) {
		k = -1;
		for( j = 0; j < marker_num; j++ ) {
	        if( object[i].id == marker_info[j].id) {

				/* you've found a pattern */
				//printf("Found pattern: %d ",patt_id);
				glColor3f( 0.0, 1.0, 0.0 );
				argDrawSquare(marker_info[j].vertex,0,0);

				if( k == -1 ) k = j;
		        else /* make sure you have the best pattern (highest confidence factor) */
					if( marker_info[k].cf < marker_info[j].cf ) k = j;
			}
		}
		if( k == -1 ) {
			object[i].visible = 0;
			continue;
		}
		
		/* calculate the transform for each marker */
		if( object[i].visible == 0 ) {
            arGetTransMat(&marker_info[k],
                          object[i].marker_center, object[i].marker_width,
                          object[i].trans);
        }
        else {
            arGetTransMatCont(&marker_info[k], object[i].trans,
                          object[i].marker_center, object[i].marker_width,
                          object[i].trans);
        }
        object[i].visible = 1;
	}
	
	//arVideoCapNext();

	/* draw the AR graphics */
    draw( object, objectnum );

	/*swap the graphics buffers*/
	argSwapBuffers();
}

static void init( void )
{
	ARParam  wparam;

    /* open the video path */
    if( arVideoOpen( vconf ) < 0 ) exit(0);
    /* find the size of the window */
    if( arVideoInqSize(&xsize, &ysize) < 0 ) exit(0);
    printf("Image size (x,y) = (%d,%d)\n", xsize, ysize);

    /* set the initial camera parameters */
    if( arParamLoad(cparam_name, 1, &wparam) < 0 ) {
        printf("Camera parameter load error !!\n");
        exit(0);
    }
    arParamChangeSize( &wparam, xsize, ysize, &cparam );
    arInitCparam( &cparam );
    printf("*** Camera Parameter ***\n");
    arParamDisp( &cparam );

	/* load in the object data - trained markers and associated bitmap files */
    if( (object=read_ObjData(model_name, &objectnum)) == NULL ) exit(0);
    printf("Objectfile num = %d\n", objectnum);
	printf("--------------------------------------\n");

    /* open the graphics window */
    argInit( &cparam, 1.0, 0, 0, 0, 0 );
}

/* cleanup function called when program exits */
static void cleanup(void)
{
	arVideoCapStop();
    arVideoClose();
    argCleanup();
}

/* draw the the AR objects */
static int draw( ObjectData_T *object, int objectnum )
{
    int     i;
    double  gl_para[16];
       
	glClearDepth( 1.0 );
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_LIGHTING);

    /* calculate the viewing parameters - gl_para */
    for( i = 0; i < objectnum; i++ ) {
        if( object[i].visible == 0 ) continue;
        argConvGlpara(object[i].trans, gl_para);
        draw_object( object[i].id, gl_para);
    }
     
	glDisable( GL_LIGHTING );
    glDisable( GL_DEPTH_TEST );
	
    return(0);
}

/* draw the user object */
static int  draw_object( int obj_id, double gl_para[16])
{
	// barvy
    GLfloat   mat_ambient1[]			= {0.8, 0.4, 0.2, 1.0};
    GLfloat   mat_flash1[]				= {0.8, 0.4, 0.2, 1.0};
	GLfloat   mat_ambient2[]			= {0.0, 0.0, 1.0, 1.0};
    GLfloat   mat_flash2[]				= {0.0, 0.0, 1.0, 1.0};
	GLfloat   mat_ambient3[]			= {0.0, 1.0, 0.0, 1.0};
    GLfloat   mat_flash3[]				= {0.0, 1.0, 0.0, 1.0};
	GLfloat   mat_ambient4[]			= {1.0, 1.0, 0.0, 1.0};
    GLfloat   mat_flash4[]				= {1.0, 1.0, 0.0, 1.0};

    // pozice svetel
    //GLfloat   light_position[]  = {100.0, -200.0, 200.0, 0.0};
	GLfloat   light_position1[]  = {0.0, 0.0, 0.0, 60.0};
	GLfloat   light_position2[]  = {0.0, 0.0, 0.0, 60.0};
	GLfloat   light_position3[]  = {0.0, 0.0, 0.0, 60.0};
	GLfloat   light_position4[]  = {0.0, 0.0, 0.0, 60.0};
    GLfloat   ambi[]            = {0.1, 0.1, 0.1, 0.1};
    GLfloat   lightZeroColor[]  = {0.6, 0.6, 0.6, 0.1};
	GLfloat   mat_flash_shiny[] = {10.0};
 
	// nastaveni pohledu objektu
    argDrawMode3D();
    argDraw3dCamera( 0, 0 );
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixd( gl_para );

 	// nastaveni svetla
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambi);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightZeroColor);

	// nastaveni materialu
    glMaterialfv(GL_FRONT, GL_SHININESS, mat_flash_shiny);

	switch(obj_id)
	{
		case 0:
			glMaterialfv(GL_FRONT, GL_SPECULAR, mat_flash1);
			glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient1);
			glTranslatef( 0.0, 0.0, 20.0 );
			glRotatef(90.0, 1.0, 0.0, 0.0);
			glRotatef(gDrawRotateAngle, 0.0, 1.0, 0.0);
			glutSolidTeapot(30);
			break;

		case 1:
			glMaterialfv(GL_FRONT, GL_SPECULAR, mat_flash2);
			glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient2);
			glTranslatef( 0.0, 0.0, 20.0 );
			glRotatef(gDrawRotateAngle, 0.0, 0.0, 1.0);
			glutSolidCube(40);
			break;

		case 2:
			glMaterialfv(GL_FRONT, GL_SPECULAR, mat_flash3);
			glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient3);
			glTranslatef( 0.0, 0.0, 10.0 );
			glRotatef(gDrawRotateAngle, 0.0, 0.0, 1.0);
			glutSolidTorus(10,30,20,20);
			break;

		case 3:
			glMaterialfv(GL_FRONT, GL_SPECULAR, mat_flash4);
			glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient4);
			glTranslatef( 0.0, 0.0, 30.0 );
			glutSolidSphere(30,20,20);

			// pozice svetla
			glLightfv(GL_LIGHT0, GL_POSITION, light_position1);
			break;

		default:
			break;
	}

    argDrawMode2D();

    return 0;
}

static void rotateAnim(float timeDelta)
{
	if (gDrawRotate) {
		gDrawRotateAngle += timeDelta * 30.0f; // rychlost otaceni za sekundu
		if (gDrawRotateAngle > 360.0f) gDrawRotateAngle -= 360.0f;
	}
}
