/* SCE CONFIDENTIAL
 * PlayStation(R)Vita Programmer Tool Runtime Library Release 02.000.081
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

/*	

	This sample shows how to initialize libdbgfont (and libgxm),
	and render debug font with triangle for clear the screen.

	This sample is split into the following sections:

		1. Initialize libdbgfont
		2. Initialize libgxm
		3. Allocate display buffers, set up the display queue
		4. Create a shader patcher and register programs
		5. Create the programs and data for the clear
		6. Start the main loop
			7. Update step
			8. Rendering step
			9. Flip operation and render debug font at display callback
		10. Wait for rendering to complete
		11. Destroy the programs and data for the clear triangle
		12. Finalize libgxm
		13. Finalize libdbgfont

	Please refer to the individual comment blocks for details of each section.
*/

// ############################ INCLUDES ############################

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sceerror.h>
#include <gxm.h>
#include <kernel.h>
#include <ctrl.h>
#include <display.h>
#include <libdbg.h>

#include <math.h>
#include <vectormath.h>

#include <libsysmodule.h>
#include <razor_capture.h>
#include <libdbgfont.h>
#include <touch.h>

#include <sce_geometry.h>

// NAMESPACE FOR VECTOR AND MATRIX OPERATIONS + METHODS (Array Of Structures)
using namespace sce::Vectormath::Simd::Aos;


// ############################ DEFINES ############################


/*	Define the debug font pixel color format to render to. */
#define DBGFONT_PIXEL_FORMAT		SCE_DBGFONT_PIXELFORMAT_A8B8G8R8


/*	Define the width and height to render at the native resolution */
#define DISPLAY_WIDTH				960
#define DISPLAY_HEIGHT				544
#define DISPLAY_STRIDE_IN_PIXELS	1024

/*	Define the libgxm color format to render to.
	This should be kept in sync with the display format to use with the SceDisplay library.
*/
#define DISPLAY_COLOR_FORMAT		SCE_GXM_COLOR_FORMAT_A8B8G8R8
#define DISPLAY_PIXEL_FORMAT		SCE_DISPLAY_PIXELFORMAT_A8B8G8R8

/*	Define the number of back buffers to use with this sample.  Most applications
	should use a value of 2 (double buffering) or 3 (triple buffering).
*/
#define DISPLAY_BUFFER_COUNT		3

/*	Define the maximum number of queued swaps that the display queue will allow.
	This limits the number of frames that the CPU can get ahead of the GPU,
	and is independent of the actual number of back buffers.  The display
	queue will block during sceGxmDisplayQueueAddEntry if this number of swaps
	have already been queued.
*/
#define DISPLAY_MAX_PENDING_SWAPS	2

/*	Helper macro to align a value */
#define ALIGN(x, a)					(((x) + ((a) - 1)) & ~((a) - 1))

/*	The build process for the sample embeds the shader programs directly into the
	executable using the symbols below.  This is purely for convenience, it is
	equivalent to simply load the binary file into memory and cast the contents
	to type SceGxmProgram.
*/


// ############################ EXTERNS ############################


// !! Data related to rendering clear vertex.
extern const SceGxmProgram binaryClearVGxpStart;
extern const SceGxmProgram binaryClearFGxpStart;

// !! Data related to rendering vertex.
extern const SceGxmProgram binaryBasicVGxpStart;
extern const SceGxmProgram binaryBasicFGxpStart;


// ############################ ENUMS ############################

/* Cube sides as number */
enum CubeSideCode
{
	LEFT  = 0,
	RIGHT = 1,
	DOWN  = 2,
	TOP   = 3,
	BACK  = 4,
	FRONT = 5
};

/* Cube-Face Colors 
   FORMAT: [alpha-blue-green-red] (0xaabbggrr)
*/
enum FaceColors
{
	// six sides of rubiks cube
	RED		= 0xff0000ff,
	GREEN	= 0xff00ff00,
	BLUE	= 0xffff0000,
	YELLOW	= 0xff00ffff,
	ORANGE	= 0xff00a5ff,
	WHITE	= 0xffffffff,
	// more colors
	BLACK	= 0xff000000,
	CYAN	= 0xffffff00,
	MAGENTA	= 0xffff00ff
};


/* what the program is doing at the moment */
enum CubeState
{
	WAIT_FOR_INPUT_STATE,
	ROTATE_CUBE_STATE,
	ROTATE_AXIS_STATE,
	TOUCH_CUBE_STATE
};


// define uint32_t format as Color
typedef uint32_t Color;



// ########################## TYPEDEFS (1) ##########################

// -------------------------------------- CLEAR VERTEX
/*	Data structure for clear geometry */
typedef struct ClearVertex
{
	float x;
	float y;
} ClearVertex;


// -------------------------------------- BASIC VERTEX
/*	Data structure for basic geometry */
typedef struct BasicVertex
{
	// position vector
	float position[3];
	// normal vector
	float normal[3];
	// color
	Color color; 
	// bool for vertex-shader; when vertex is rotating
	unsigned char rotate;
} BasicVertex;



// forward declaration for cube axis
struct CubeAxis;

// -------------------------------------- CUBE FACE
/* Data structure for cube faces with attributes
	1. 4 pointers on vertices (2 triangles)
	2. 2 pointers on cube axes 
	3. 2 integers for the position in the axes
	4. color of the face
*/
typedef struct CubeFace
{
	BasicVertex* vertices[4];
	CubeAxis* axis1;
	CubeAxis* axis2;

	int axis1Pos;
	int axis2Pos;

	Color color;

	// change color in every vertex
	void setColor(Color c) {
		color = c;
		for(int i = 0; i < 4; ++i)
			vertices[i]->color = c;
	}

	// change color of 2 edges for highlighting
	void setCurrent(bool c) {
		vertices[0]->color = c ? CYAN : color;	
		vertices[2]->color = c ? MAGENTA : color;
	}

} CubeFace;


// -------------------------------------- CUBE SIDE
/* Data structure for cube sides with attributes
	1. 9 faces (3x3)
	2. vector for the side normal
	3. vector for the side origin
	4. vector for local x-axis
	5. vector for local y-axis
	6. bool for rotating (when edge-axis is rotating and this is the attached cubeside)

	ORDER:
			[3]T
			0 3 6
			1 4 7
			6 7 8
	[0]L	[5]F	[1]R	[4]B
	2 5 8	6 7 8	8 5 2	8 7 6
	1 4 7	3 4 5	7 4 1	5 4 3
	0 3 6	0 1 2	6 3 0	2 1 0
			[2]D
			2 5 8
			1 4 7
			0 3 6
*/
typedef struct CubeSide
{
	CubeFace faces[9];

	Vector3 normal;
	Vector3 origin;

	Vector3 localXDim;
	Vector3 localYDim;

	bool rotating;

	// set the rotate attribute of every vertex on the cubeside
	void setRotate(bool r)
	{
		rotating = r;

		// for every face
		for(int i = 0; i < 9; ++i)
		{
			CubeFace *f = &faces[i];
			// and every vertex on the face
			for(int j = 0; j < 4; ++j)
			{
				BasicVertex* v = (f->vertices[j]);
				v->rotate = r;
			}
		}
	}

	// rotate colors of cubeside around center after rotate animation
	void shift(bool clockwise)
	{
		// temporary field with new colors
		Color tmp[9];

		int row = 0; // row counter
		int col = 0; // column counter
		// for every face
		for(int i = 0; i < 9; ++i)
		{
			/*
			   [0]1 2	  [6]3 0
				3 4 5  ->  7 4 1
				6 7 8      8 5 2
				
				change order of faces clockwise
				0,1,2 -> 6,3,0
				so we get the formular:
				6 (startIndex) - column*3 + row
			*/
			int newIndex = 6 - (col*3) + row;

			/*
			   [0]1 2	  [2]5 8
				3 4 5  ->  1 4 7
				6 7 8      0 3 6
				
				change order of faces counterclockwise
				0,1,2 -> 2,5,8
				so we get the formular:
				2 (startIndex) + column*3 - row
			*/
			if(clockwise)
				newIndex = 2 + (col*3) - row;

			// save new color of face in tmp
			tmp[i] = faces[newIndex].color;

			// if column counter is at the end (3), increment row and reset column
			if(++col >= 3) {
				col -= 3;
				row ++;
			}
		}
		
		// apply new colors on faces
		for(int i = 0; i < 9; ++i) {
			faces[i].setColor(tmp[i]);
		}
	}

} CubeSide;

// ####################### STATIC VARIABLES (1) #######################

// cube has 6 sides
static CubeSide sides[6];

// the current highlighted face to the center face of the front side
static CubeFace* currentFace  = &(sides[FRONT].faces[4]);

// the side of the current face
static int currentCubeSide;

// is a cube side rotating?
static bool isRotating = false;

/* rotation matrix for basic vertex uniform param
   for all vertices on a currently rotating axis/side */
static Matrix4 currentRotation;

/* for currently rotating faces (angles in DEG)
	- rotateAngle: current angle of rotation
	- targetAngle: end-angle of rotation
	- previousAngle: needed for touch-drag (this + distance of moved finger = rotateAngle)
*/
static float rotateAngle   = 0.0f, 
			 targetAngle   = 0.0f, 
			 previousAngle = 0.0f;



// ########################## TYPEDEFS (2) ##########################

// -------------------------------------- CUBE SIDE
/* Data structure for cube axes with attributes
	1. 12 pointers on cube faces
	2. pointer on attached cube side (only for border axes [0],[2])
	3. axis as integer (0 = x, 1 = y, 2 = z)
	4. bool for rotating (push faces +3/-3 after rotate animation)

	CubeAxis [3 axis][3 rows]
			 [0 = X]
			 [1 = Y]
			 [2 = Z]

	-----------------------------
				Y[1]
			Y[0]| Y[2]
			  v v v	   [RIGHT]		
		X[0]->6 7 8    	8 5 2
		X[1]->3 4 5	   	7 4 1
		X[2]->0 1 2	   	6 3 0
			 [FRONT]   	^ ^ ^
					  Z[2]| Z[0]
					   	Z[1]	
				
*/
typedef struct CubeAxis
{
	CubeFace* faces[12];
	CubeSide* borderSide;

	int direction;

	bool rotating;

	// constructor presets some attributes
	CubeAxis()
	{
		rotating = false;
		direction = -1; // no possible axis (only 0,1,2 allowed)
	}

	/* push faces +3/-3 after rotate animation
	   @param reverse: push faces +3 or -3
	*/
	void shift(bool reverse)
	{
		// reset highlighted face
		currentFace->setCurrent(false);

		// preset index of current face 
		int currentFaceIndex = -1;
		
		// temporary field with new colors
		Color tmp[12];

		// write new colors in tmp
		for(int i = 0; i < 12; ++i)
		{
			/*
				no value over 11 and under 0 allowed
				so if we shift +3 -> modulo 12
				and if we shift -3 -> +12 (because no negative values are allowed) modulo 12
			*/
			int newIndex = (reverse ? (i+3)%12 : (i+12-3)%12);
			// save new color for face
			tmp[i] = faces[newIndex]->color;
		}
		
		// apply new colors on faces
		for(int i = 0; i < 12; ++i) {

			faces[i]->setColor(tmp[i]);
			// determine index of current face on axis
			if(faces[i] == currentFace)
				currentFaceIndex = i;
		}
		
		// if current face is on axis -> set pointer on face +3 / -3 (+12 modulo 12 like above^)
		if(currentFaceIndex >= 0) 
			currentFace = (faces[(currentFaceIndex + (reverse ? -3 : 3) + 12) % 12]);

		// if the axis touches a cube side -> rotate their colors too
		if(borderSide != NULL)
			borderSide->shift(reverse);

		// highlight current face
		currentFace->setCurrent(true);
	}

	
	// set the rotate attribute of every vertex on the cube axis
	void setRotate(bool r)
	{
		rotating = r;

		// for every face
		for(int i = 0; i < 12; ++i)
		{
			CubeFace *f = faces[i];
			// and every vertex on the face
			for(int j = 0; j < 4; ++j)
			{
				BasicVertex* v = (f->vertices[j]);
				v->rotate = r;
			}
		}

		// if the axis touches a cube side -> set their rotation attribute too
		if(borderSide != NULL)
			borderSide->setRotate(r);
	}

} CubeAxis;


/*	Data structure to pass through the display queue.  This structure is
	serialized during sceGxmDisplayQueueAddEntry, and is used to pass
	arbitrary data to the display callback function, called from an internal
	thread once the back buffer is ready to be displayed.

	In this example, we only need to pass the base address of the buffer.
*/
typedef struct DisplayData
{
	void *address;
} DisplayData;



// ####################### STATIC VARIABLES (2) #######################


/*  libgxm data */
static SceGxmContextParams		s_contextParams;			/* libgxm context parameter */
static SceGxmRenderTargetParams s_renderTargetParams;		/* libgxm render target parameter */
static SceGxmContext			*s_context			= NULL;	/* libgxm context */
static SceGxmRenderTarget		*s_renderTarget		= NULL;	/* libgxm render target */
static SceGxmShaderPatcher		*s_shaderPatcher	= NULL;	/* libgxm shader patcher */

/*	display data */
static void							*s_displayBufferData[ DISPLAY_BUFFER_COUNT ];
static SceGxmSyncObject				*s_displayBufferSync[ DISPLAY_BUFFER_COUNT ];
static int32_t						s_displayBufferUId[ DISPLAY_BUFFER_COUNT ];
static SceGxmColorSurface			s_displaySurface[ DISPLAY_BUFFER_COUNT ];
static uint32_t						s_displayFrontBufferIndex = 0;
static uint32_t						s_displayBackBufferIndex = 0;
static SceGxmDepthStencilSurface	s_depthSurface;

/*	shader data */
static int32_t					s_clearVerticesUId;
static int32_t					s_clearIndicesUId;
static SceGxmShaderPatcherId	s_clearVertexProgramId;
static SceGxmShaderPatcherId	s_clearFragmentProgramId;
static SceGxmShaderPatcherId	s_basicVertexProgramId;
static SceGxmShaderPatcherId	s_basicFragmentProgramId;
static SceUID					s_patcherFragmentUsseUId;
static SceUID					s_patcherVertexUsseUId;
static SceUID					s_patcherBufferUId;
static SceUID					s_depthBufferUId;
static SceUID					s_vdmRingBufferUId;
static SceUID					s_vertexRingBufferUId;
static SceUID					s_fragmentRingBufferUId;
static SceUID					s_fragmentUsseRingBufferUId;
static ClearVertex				*s_clearVertices			= NULL;
static uint16_t					*s_clearIndices				= NULL;
static SceGxmVertexProgram		*s_clearVertexProgram		= NULL;
static SceGxmFragmentProgram	*s_clearFragmentProgram		= NULL;
static SceGxmVertexProgram		*s_basicVertexProgram		= NULL;
static SceGxmFragmentProgram	*s_basicFragmentProgram		= NULL;
static BasicVertex				*s_basicVertices			= NULL;
static uint16_t					*s_basicIndices				= NULL;
static int32_t					s_basicVerticesUId;
static int32_t					s_basicIndiceUId;


//!! The program parameter for the transformation of the triangle
static float s_wvpData[16];
static const SceGxmProgramParameter *s_wvpParam = NULL;
static const SceGxmProgramParameter *s_rotParam = NULL;
static const SceGxmProgramParameter *s_rotateParam = NULL;

/* Callback function to allocate memory for the shader patcher */
static void *patcherHostAlloc( void *userData, uint32_t size );

/* Callback function to allocate memory for the shader patcher */
static void patcherHostFree( void *userData, void *mem );

/*	Callback function for displaying a buffer */
static void displayCallback( const void *callbackData );

/*	Helper function to allocate memory and map it for the GPU */
static void *graphicsAlloc( SceKernelMemBlockType type, uint32_t size, uint32_t alignment, uint32_t attribs, SceUID *uid );

/*	Helper function to free memory mapped to the GPU */
static void graphicsFree( SceUID uid );

/* Helper function to allocate memory and map it as vertex USSE code for the GPU */
static void *vertexUsseAlloc( uint32_t size, SceUID *uid, uint32_t *usseOffset );

/* Helper function to free memory mapped as vertex USSE code for the GPU */
static void vertexUsseFree( SceUID uid );

/* Helper function to allocate memory and map it as fragment USSE code for the GPU */
static void *fragmentUsseAlloc( uint32_t size, SceUID *uid, uint32_t *usseOffset );

/* Helper function to free memory mapped as fragment USSE code for the GPU */
static void fragmentUsseFree( SceUID uid );


/*	@brief User main thread parameters */
extern const char			sceUserMainThreadName[]		= "simple_main_thr";
extern const int			sceUserMainThreadPriority	= SCE_KERNEL_DEFAULT_PRIORITY_USER;
extern const unsigned int	sceUserMainThreadStackSize	= SCE_KERNEL_STACK_SIZE_DEFAULT_USER_MAIN;

/*	@brief libc parameters */
unsigned int	sceLibcHeapSize	= 1*1024*1024;

// position relative cube rotating with quaternion
static Quat m_orientationQuat(1.0f, 0.0f, 0.0f, 0.0f);
// matrix for world view position; later used as uniform parameter in vertex shader
static Matrix4 finalTransformation;
// matrix for cube rotation; later used as uniform parameter in vertex shader
static Matrix4 finalRotation;

// current state; determines what cube is doing; (-> enum CubeState)
static int state;

// ---- FRONT TOUCH ----
// id of currently tracked touch point
static int frontTouchID = -1;
// screen coordinates of finger-down touch point
static Vector2 frontTouchStart;
// is the tracked finger on screen?
static bool frontTouched;

// ---- BACK TOUCH ----
// screen coordinates of finger-down touch point
static float backTouchXStart = 0.0f;
static float backTouchYStart = 0.0f;
// is the tracked finger on screen?
static bool  backTouchDrag = false;

// ---- BUTTONS ----
// is a rotate button pressed ? (triangle / circle / X / rectangle)
static bool buttonShiftPressed		= true;
// is a drection button pressed ? (up / down / left / right)
static bool buttonDirectionPressed	= true;


// 3x3 cube axes (3 directions x,y,z and 3 rows of directions)
static CubeAxis axis[3][3];

// ---- currently rotating faces ----
// is rotation direction positive or negative ?
static bool rotateReversed;
// rotate axis (0 = x, 1 = y, 2 = z)
static int rotateAxis;
// distance of tracked finger after touch start (in object space)
static float rotateDistance;


// ########################### PROTOTYPES  ###########################

// move the current face in the current axis
void changeCurrentFace(int );

// create a cube side
void CreateCubeSide(BasicVertex* , int , int, Color, int );

// add 3 cube faces of a cube side to an axis
void addToCubeAxis(CubeAxis* , int , int , int , int , int );


/*	@brief Main entry point for the application
	@return Error code result of processing during execution: <c> SCE_OK </c> on success,
	or another code depending upon the error
*/
int main( void );

// called every frame
void Update(void);

/*	@brief Initializes the graphics services and the libgxm graphics library
	@return Error code result of processing during execution: <c> SCE_OK </c> on success,
	or another code depending upon the error
*/
static int initGxm( void );

/*	 @brief Creates scenes with libgxm */
static void createGxmData( void );

/*	@brief Main rendering function to draw graphics to the display */
static void render( void );

/*	@brief render libgxm scenes */
static void renderGxm( void );

/*	@brief cycle display buffer */
static void cycleDisplayBuffers( void );

/*	@brief Destroy scenes with libgxm */
static void destroyGxmData( void );

/*	@brief Function to shut down libgxm and the graphics display services
	@return Error code result of processing during execution: <c> SCE_OK </c> on success,
	or another code depending upon the error
*/
static int shutdownGxm( void );


// ########################### MAIN  ###########################

/* Main entry point of program */
int main( void )
{
	// set current state 
	state = WAIT_FOR_INPUT_STATE;
	// no touch point is tracked
	frontTouched = false;

	// init return code
	int returnCode = SCE_OK;

	/* initialize libdbgfont and libgxm */
	returnCode = initGxm();
	SCE_DBG_ALWAYS_ASSERT( returnCode == SCE_OK );

	// init debug font (output on PSVita screen)
    SceDbgFontConfig config;
	memset( &config, 0, sizeof(SceDbgFontConfig) );
	config.fontSize = SCE_DBGFONT_FONTSIZE_LARGE;
	returnCode = sceDbgFontInit( &config );
	SCE_DBG_ALWAYS_ASSERT( returnCode == SCE_OK );

	/* Message for SDK sample auto test */
	printf( "## simple: INIT SUCCEEDED ##\n" );


	/* create gxm graphics data */
	createGxmData();

	// start joystick
	sceCtrlSetSamplingMode(SCE_CTRL_MODE_DIGITALANALOG_WIDE);
	// start back-touch-panel
	sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_START);
	// start front-touch-panel
	sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);

	/* 6. main loop */
	while ( true)
	{
		// call update every frame
        Update();
		// call render every frame
		render();
		// cycle display buffers
		cycleDisplayBuffers();
	}
    
	// 10. wait until rendering is done 
	sceGxmFinish( s_context );

	// destroy gxm graphics data 
	destroyGxmData();

	// shutdown libdbgfont and libgxm 
	returnCode = shutdownGxm();
	SCE_DBG_ALWAYS_ASSERT( returnCode == SCE_OK );

	// Message for SDK sample auto test
	printf( "## api_libdbgfont/simple: FINISHED ##\n" );

	return returnCode;
}


// ########################### SMALL FUNCTIONS  ###########################
// cast char to float
float makeFloat(unsigned char input)
{
    return (((float)(input)) / 255.0f * 2.0f) - 1.0f;
}

// clamp value (of joystick): only movements outside the deadzone are != 0.0f
float clampDeadzone(float v)
{
	float deadzone = 0.2f;
	return (v < deadzone && v > -deadzone) ? 0.0f : v;
}

// convert rad to deg
float rad(float deg)
{
	float pi = 3.14159265358979323846f;
	return deg * pi / 180.0f;
}

// if a direction button is pressed, change the current face to the next/previous on the axis
void changeCurrentFace(int direction)
{
	// stop current face highlight
	currentFace->setCurrent(false);

	// get the two axes of the current face
	CubeAxis* rot1 = currentFace->axis1;
	CubeAxis* rot2 = currentFace->axis2;

	// and the positions of the face on the axes
	int rot1Pos = currentFace->axis1Pos;
	int rot2Pos = currentFace->axis2Pos;

	// and increment / decrement the position of the current face
	switch(direction)
	{
	case TOP:
		currentFace = rot2->faces[(rot2Pos-1+12) % 12];
		break;
	case DOWN:
		currentFace = rot2->faces[(rot2Pos+1) % 12];
		break;
	case LEFT:
		currentFace = rot1->faces[(rot1Pos-1+12) % 12]; 
		break;
	case RIGHT:
		currentFace = rot1->faces[(rot1Pos+1) % 12];
		break;
	default:
		return;
	}
	// highlight current face
	currentFace->setCurrent(true);		
}


// ########################### ROTATE AXIS  ###########################

// determine the rotation matrix, rotate axis and rotate direction 
void startRotate(int direction, bool reverse)
{
	state = ROTATE_AXIS_STATE;

	// so update function knows if vertices are rotating
	isRotating = true;
	// rotate positive or negative direction?
	rotateReversed = reverse;
	// rotation axis (0 = x, 1 = y, 2 = z)
	rotateAxis = direction;
	
	// start angle is 0 and we wish to rotate 90deg positive or negative
	targetAngle = rotateAngle + (reverse ? -90.0f : 90.0f);

	// construct rotation matrix over (rad) angle
	if(direction == 0) 
		currentRotation = Matrix4::rotationY(rad(rotateAngle));
	else if(direction == 1)
		currentRotation = Matrix4::rotationX(rad(rotateAngle));
	else if(direction == 2)
		currentRotation = Matrix4::rotationZ(rad(rotateAngle));
}

// gets local cube side coordinates and returns a cubeface, if one was hit by the raycast
CubeFace* getTouchedCubeFace(int cubeSide, float lx, float ly)
{
	CubeSide* s = &sides[cubeSide];

	// top left corner of cube side
	float dx = -0.6f;
	float dy = -0.6f;

	int cubeFaceCount = 0;

	// check every row
	for(int i = 0; i < 3; ++i)
	{
		// and column
		for(int j = 0; j < 3; ++j)
		{
			// 0.4 = width and height of a cube face
			if(lx > dx && lx < (dx+0.4f) 
			&& ly > dy && ly < (dy+0.4f)) 
			{
				return &(s->faces[cubeFaceCount]);
			}

			cubeFaceCount ++;
			dx += 0.4f;
		}
		dy += 0.4f;
		dx = -0.6f;
	}
	// no face was touched
	return NULL;
}

/* finger down event of front touch screen
   we cast a ray through the scene on finger position
   and check if it touches a cube face
   then we change the current face
*/
bool castRay(Vector2 touchPos)
{
	// generate 2 points from touched position with 0.8 digits length through the szene
	// start point
	Vector4 p1(touchPos.getX(), touchPos.getY(), 0.1f, 1.0f);
	// end point
	Vector4 p2(touchPos.getX(), touchPos.getY(), 0.9f, 1.0f);

	// transform to object space
	p1 = inverse(finalTransformation) * p1;
	p2 = inverse(finalTransformation) * p2;

	// normalize
	p1 /= p1.getW();
	p2 /= p2.getW();

	// change vectors to points
	Point3 pointStart(p1.getXYZ());
	Point3 pointEnd(p2.getXYZ());

	// generate a ray
	sce::Geometry::Aos::Ray touchRay(pointStart, pointEnd);

	// calculate the difference vector
	Vector4 d = p2 - p1;

	// test every cube side for collision
	for(int i = 0; i < 6; ++i)
	{
		// cube side origin vector
		Vector3 o = sides[i].origin;
		// cube side normal vector
		Vector3 n = sides[i].normal;

		// calculate dot product of normal and ray
		float frontHitFactor = dot(touchRay.getDirection(), n);

		// ray origin vector
		Vector3 touchRayOrigin(touchRay.getOrigin());
		// ray direction vector
		Vector3 touchRayDirection(touchRay.getDirection());

		// if dot product is lower 0 -> ray is looking at the front side of the side
		if(frontHitFactor < 0)
		{
			// alpha value for hesse normal form
			float a = dot(o - touchRayOrigin, n) / dot(touchRayDirection, n);

			// get point of intersection between ray and cube side layer
			Vector3 intersection(touchRay.getPointOnRay(sce::Vectormath::Simd::floatInVec(a)));

			// get cube side local coordinate system x (u) and y (v)
			Vector3 u = sides[i].localXDim;
			Vector3 v = sides[i].localYDim;

			// transform intersection point to local cube side coordinates
			float xIntersection = dot(u, intersection - o);
			float yIntersection = dot(v, intersection - o);

			// if the touch point intersects the side
			if(xIntersection > -0.6f && xIntersection < 0.6f
			&& yIntersection > -0.6f && yIntersection < 0.6f)
			{
				// get the touched cube face
				CubeFace* touchedCubeFace = getTouchedCubeFace(i, xIntersection, yIntersection);
				if(touchedCubeFace != NULL)
				{
					// change current face to touched cube face
					currentFace->setCurrent(false);
					currentFace = touchedCubeFace;
					currentFace->setCurrent(true);

					// set touch start vector
					frontTouchStart = Vector2(xIntersection, yIntersection);

					// set current cube side variable
					currentCubeSide = i;

					// FOR DEBUGGING
					//printf("axis1: %i, axis2: %i\n", currentFace->axis1->direction, currentFace->axis2->direction);

					// we don't want to turn yet, so we set the rotation axis to an invalid value
					rotateAxis = -1;
					
					// leave the function
					return true;
				}
			}	
			
		}		
	}
	// no cube side was touched
	return false;
}

/* because every cube side has different local coordinates
   the coordinate system is turned on left and right cube side
*/
void setRotateAxis(bool xDirection, bool reversed)
{
	// xDirection = rotating the localXDim or localYDim ?
	// reversed = rotate direction (positive or negative) ?

	switch(currentCubeSide)
	{
	case FRONT: case BACK: case TOP: case DOWN:
		// set rotate axis
		rotateAxis = (xDirection ? currentFace->axis1->direction : currentFace->axis2->direction);
		// set rotate direction
		rotateReversed = reversed;
		break;

	case LEFT: case RIGHT:
		// set rotate axis
		rotateAxis = (xDirection ? currentFace->axis2->direction : currentFace->axis1->direction);
		// set rotate direction
		rotateReversed = !reversed;
		break;
	}

	// the rotate direction is inversed on these sides
	switch(currentCubeSide)
	{
	case RIGHT: case DOWN: case BACK:
		rotateReversed = !rotateReversed;
		break;
	}
}

/* same problem as above:
 some sides have different coordinate systems so we need to use different
 distances of the moved finger (x-distance / y-distance)
*/
void setRotateDistance(bool reversed, float distanceX, float distanceY)
{
	rotateDistance = (reversed ? distanceX : distanceY);

	switch(currentCubeSide)
	{
	case BACK:
		rotateDistance = (reversed ? -distanceX : -distanceY);
		break;
	case LEFT: 
		rotateDistance = (reversed ? -distanceY : -distanceX);
		break;
	case DOWN:
		rotateDistance = (reversed ? -distanceX : -distanceY);
		break;
	case RIGHT:
		rotateDistance = (reversed ?  distanceY : distanceX);
		break;
	}
}

/* called every frame when tacked finger is on screen
 we check if the distance of the moved finger is above a value
 and change the rotation distance for later angular calculations
*/
void rotateAxisWithTouch(Vector2 touchPos)
{
	// generate 2 points from touched position with 0.8 digits length through the szene
	// start point
	Vector4 p1(touchPos.getX(), touchPos.getY(), 0.1f, 1.0f);
	// end point
	Vector4 p2(touchPos.getX(), touchPos.getY(), 0.9f, 1.0f);

	// transform to object space
	p1 = inverse(finalTransformation) * p1;
	p2 = inverse(finalTransformation) * p2;

	// normalize
	p1 /= p1.getW();
	p2 /= p2.getW();

	// change vectors to points
	Point3 pointStart(p1.getXYZ());
	Point3 pointEnd(p2.getXYZ());

	// generate a ray
	sce::Geometry::Aos::Ray touchRay(pointStart, pointEnd);

	// calculate the difference vector
	Vector4 d = p2 - p1;

	// origin vector of current cube side
	Vector3 o = sides[currentCubeSide].origin;
	// normal vector of current cube side
	Vector3 n = sides[currentCubeSide].normal;

	// calculate dot product of normal and ray
	float frontHitFactor = dot(touchRay.getDirection(), n);

	// ray origin vector
	Vector3 touchRayOrigin(touchRay.getOrigin());
	// ray direction vector
	Vector3 touchRayDirection(touchRay.getDirection());

	// alpha value for hesse normal form
	float a = dot(o - touchRayOrigin, n) / dot(touchRayDirection, n);

	// get point of intersection between ray and cube side layer
	Vector3 intersection(touchRay.getPointOnRay(sce::Vectormath::Simd::floatInVec(a)));

	// get cube side local coordinate system x (u) and y (v)
	Vector3 u = sides[currentCubeSide].localXDim;
	Vector3 v = sides[currentCubeSide].localYDim;

	// transform intersection point to local cube side coordinates
	float xIntersection = dot(u, intersection - o);
	float yIntersection = dot(v, intersection - o);

	// calculate moved distance of finger
	float distanceX = (xIntersection) - (float)(frontTouchStart.getX());
	float distanceY = (yIntersection) - (float)(frontTouchStart.getY());

	// if we aren't rotating yet, rotateAxis has an invalid value = -1
	if(rotateAxis == -1)
	{
		// if we move more than 0.08 digits in cube side space in x or y direction
		// we lock the rotate axis -> set rotate axis
		if(abs(distanceX) > 0.08f && abs(distanceX) > abs(distanceY)) {
			setRotateAxis(true, distanceX < 0);
		}
		else if(abs(distanceY) > 0.08f && abs(distanceY) > abs(distanceX)) {
			setRotateAxis(false, distanceY > 0);
		}
	}
	
	// if we are rotating, we set the local x or y distance as distance for rotation
	if(rotateAxis >= 0)
	{
		setRotateDistance((rotateAxis == currentFace->axis1->direction), distanceX, -distanceY);
		isRotating = true;
	}
}



// ########################### UPDATE  ###########################

void Update (void)
{
	// get touch data from back panel
	SceTouchData pDataB;
	int resultBackTouch = sceTouchRead(SCE_TOUCH_PORT_BACK, &pDataB, 1);

	// get touch data from front panel
	SceTouchData pDataF;
	int resultFrontTouch = sceTouchRead(SCE_TOUCH_PORT_FRONT, &pDataF, 1);

	// get button data
	SceCtrlData buttonResult;
	sceCtrlReadBufferPositive(0, &buttonResult, 1);


	if(state == WAIT_FOR_INPUT_STATE ||state == ROTATE_CUBE_STATE)
	{
		// TOUCH BACK -----------------------------------

		// number of touch points
		int reportNum = pDataB.reportNum;

		float pX = 0.0f;
		float pY = 0.0f;

		if(reportNum > 0)
		{
			// we are rotating the cube
			state = ROTATE_CUBE_STATE;

			// finger down event
			if(!backTouchDrag)
			{
				// calculate touch point coordinates in world space (-1 to +1)
				backTouchXStart = pDataB.report[0].x / 1919.f * 2.f - 1;
				backTouchYStart = 1- pDataB.report[0].y / 889.f * 2.f;

				// now we are dragging
				backTouchDrag = true;
			} 
			// finger drag event
			else 
			{
				// calculate touch point coordinates in world space (-1 to +1)
				pX = pDataB.report[0].x / 1919.f * 2.f - 1;
				pY = 1- pDataB.report[0].y / 889.f * 2.f;
				
				// subtract start point of finger
				pX -= backTouchXStart;
				pY -= backTouchYStart;
			}
			
		} 
		// no tracked touch point found
		else 
		{
			backTouchDrag = false;
			state = WAIT_FOR_INPUT_STATE;
		}


		// CUBE ROTATION (+JOYSTICK) -----------------------------------

		/* calculate rotation values for different axes
		   if no tracked touch point on the back is used for the rotation
		   we get the distance from the left joystick
		*/
		float rX = backTouchDrag ? -pY*3 : clampDeadzone(makeFloat(buttonResult.ly));
		float rY = backTouchDrag ? -pX*3 : clampDeadzone(-makeFloat(buttonResult.lx));
		// z-rotation always from joystick because the touch panel is only 2-dimensional
		float rZ = clampDeadzone(makeFloat(buttonResult.rx));

		// if we rotate per joystick, the state wasn't changed by touch back event
		if(state == WAIT_FOR_INPUT_STATE) {
			// check if a joystick axis is out of the deadzone and change state
			if(rX != 0.0f || rY != 0.0f || rZ != 0.0f)
				state = ROTATE_CUBE_STATE;
		}

		// create a quaternion with our calculated rotation distances
		Quat rotationVelocity(rX*0.01f, rY*0.01f, rZ*0.01f, 0.0f);
		// calculate the new value for our static rotation quaternion
		m_orientationQuat += 2.5f * rotationVelocity * m_orientationQuat;
		// and normalize
		m_orientationQuat = normalize(m_orientationQuat);

		// construct the 4x4 matrix from the unit-length quaternion and a 3D vector
		finalRotation = Matrix4(m_orientationQuat, Vector3(0.0f, 0.0f, 0.0f));
		// construct viewing matrix based on eye position, position looked at, and up direction
		Matrix4 lookAt = Matrix4::lookAt(Point3(0.0f, 0.0f, -3.0f), Point3(0.0f, 0.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f));
		// construct a perspective projection matrix with params(fovyRadians, aspectRatio, zNear, zFar)
		Matrix4 perspective = Matrix4::perspective(
			3.141592f / 4.0f,
			(float)DISPLAY_WIDTH / (float)DISPLAY_HEIGHT,
			0.1f,
			10.0f
		);
		// construct our final transformation matrix for cube rotation; later used as uniform parameter in vertex shader
		finalTransformation = perspective * lookAt * finalRotation;
	}



	// TOUCH FRONT -----------------------------------

	if(state == WAIT_FOR_INPUT_STATE || state == TOUCH_CUBE_STATE)
	{
		// get number of current touch points
		int reportNum = pDataF.reportNum;
		// if no point is tracked yet, track the first touchpoint 
		if(frontTouchID == -1 && reportNum > 0) {
			frontTouchID = pDataF.report[0].id;
		}
		// no touch points on screen, set ID to -1
		else if(reportNum == 0) {
			frontTouchID = -1;
		}

		// check if tracked id exists
		bool touchIdExists = false;
		for(int i = 0; i < reportNum; ++i)
		{
			// if tracked id exists
			if(pDataF.report[i].id == frontTouchID)
			{
				// change state
				state = TOUCH_CUBE_STATE;

				// transform touch coordinates to world space
				float pX = pDataF.report[i].x / 1919.f * 2.f - 1;
				float pY = 1- pDataF.report[i].y / 1087.f * 2.f;

				touchIdExists = true;

				// drag event
				if(frontTouched) {
					rotateAxisWithTouch(Vector2(pX, pY));
				} 
				// finger down event
				else {
					frontTouched = castRay(Vector2(pX, pY));
				}
			}
		}

		// tracked touch id doesn't exists
		if(!touchIdExists) 
		{
			frontTouched = false;

			// if the finger moved an axis and was then released
			if(rotateAngle != 0)
			{
				/* calculate angle for snap
				   for example:
				   rotateAngle = 95deg -> we can snap to lower angle 90 or higher angle 180
				   90 is closer to 95 than 180, so we snap back to 90deg
				*/
				int tmpAngle	= (int)(abs(rotateAngle)) % 90;
				int lowerAngle  = (int)(abs(rotateAngle)) - tmpAngle;
				int higherAngle = (int)(abs(rotateAngle)) - tmpAngle + 90;
				
				// angle negative or positive?
				rotateReversed = rotateAngle < 0;
				// take lower or highter angle and flag it (we used the absolute value before)
				targetAngle = (float)((tmpAngle < 45 ? lowerAngle : higherAngle) * (rotateReversed ? -1 : 1));

				// FOR DEBUGGING
				//printf("angle: %f, tmp: %i, lower: %i, higher: %i, target: %f\n", rotateAngle, tmpAngle, lowerAngle, higherAngle, targetAngle);
				//if(rotateReversed) printf("rotateReversed: true\n");
				//else printf("rotateReversed: false\n");

				// calculate if the target angle is lower than the current angle and set rotate direction
				rotateReversed = (targetAngle < rotateAngle);

				// change state
				state = ROTATE_AXIS_STATE;
			}
			// if the finger hasn't moved an axis
			else state = WAIT_FOR_INPUT_STATE;
		}
	}
	 
	

	// BUTTONS -----------------------------------
	if(state == WAIT_FOR_INPUT_STATE || state == ROTATE_AXIS_STATE)
	{
		// get two possible axes of current face
		CubeAxis* rot1 = currentFace->axis1;
		CubeAxis* rot2 = currentFace->axis2;

		// and rotate in the direction of the buttons
		// > CIRCLE
		if ((buttonResult.buttons & SCE_CTRL_CIRCLE) != 0 && !isRotating) {
			if(!buttonShiftPressed)
				startRotate(rot1->direction, false);
			buttonShiftPressed = true;
		} 
		// > SQUARE
		else if ((buttonResult.buttons & SCE_CTRL_SQUARE) != 0 && !isRotating) {
			if(!buttonShiftPressed)
				startRotate(rot1->direction, true);
			buttonShiftPressed = true;
		} 
		// > TRIANGLE
		else if ((buttonResult.buttons & SCE_CTRL_TRIANGLE) != 0 && !isRotating) {
			if(!buttonShiftPressed)
				startRotate(rot2->direction, true);
			buttonShiftPressed = true;
		} 
		// > CROSS
		else if ((buttonResult.buttons & SCE_CTRL_CROSS) != 0 && !isRotating) {
			startRotate(rot2->direction, false);
			buttonShiftPressed = true;
		} 
		// NONE
		else {
			buttonShiftPressed = false;
		}
	}
	
	// DIRECTION KEYS -----------------------------------
	if(state == WAIT_FOR_INPUT_STATE)
	{
		// if a direction button (up, down, left, right) was pressed, change the currentFace
		// UP
		if ((buttonResult.buttons & SCE_CTRL_UP) != 0) {
			if(!buttonDirectionPressed) {
				changeCurrentFace(TOP);
			}
			buttonDirectionPressed = true;
		} 
		// DOWN
		else if((buttonResult.buttons & SCE_CTRL_DOWN) != 0) {
			if(!buttonDirectionPressed) {
				changeCurrentFace(DOWN);	
			}
			buttonDirectionPressed = true;
		} 
		// LEFT
		else if((buttonResult.buttons & SCE_CTRL_LEFT) != 0) {
			if(!buttonDirectionPressed) {
				changeCurrentFace(LEFT);
			}
			buttonDirectionPressed = true;
		} 
		// RIGHT
		else if((buttonResult.buttons & SCE_CTRL_RIGHT) != 0) {
			if(!buttonDirectionPressed) {
				changeCurrentFace(RIGHT);	
			}
			buttonDirectionPressed = true;
		} 
		// NONE
		else {
			buttonDirectionPressed = false;
		}
	}


	// ROTATE ----------------------------------------
	// if we are rotating per button or touch
	if((state == ROTATE_AXIS_STATE || state == TOUCH_CUBE_STATE) && isRotating)
	{
		CubeAxis* currRotateAxis;

		// get current rotation axis
		if(rotateAxis == currentFace->axis1->direction)
			currRotateAxis = currentFace->axis1;
		else
			currRotateAxis = currentFace->axis2;

		// set rotation attribute in vertices on rotation axis true
		currRotateAxis->setRotate(true);

		// increment/decrement angle every frame; only relevant for button rotation
		int angleIncrement = (rotateReversed ? -5 : 5);

		// if we are rotating < 0 we need to swap the values
		float smallerValue = (rotateReversed ? targetAngle : rotateAngle);
		float biggerValue  = (rotateReversed ? rotateAngle : targetAngle);

		// if we are rotating with buttons instead of touch
		if(state != TOUCH_CUBE_STATE)
		{
			// and the target isn't reached, increment the angle
			if(smallerValue < biggerValue)
				rotateAngle += angleIncrement;
			
			// if the target is reached
			if(smallerValue >= biggerValue) {
				// reset the rotation
				rotateAngle = 0.0f;
				// if there was a rotation
				if(targetAngle != 0) {
					// calculate how often the rotating axis has to shift colors
					int shiftCount = (abs(targetAngle) / 90);
					for(int i = 0; i < shiftCount; ++i)
						currRotateAxis->shift(targetAngle < rotateAngle);
				}
				// reset rotate variables and attributes
				isRotating = false;
				currRotateAxis->setRotate(false);
				rotateAxis = -1;
				// change state
				state = WAIT_FOR_INPUT_STATE;
			}
			// we need to set these variables because we don't rotate via touch
			rotateDistance = 0.0f;
			previousAngle = rotateAngle;
		}
		/* if we rotate with buttons, rotateDistance is 0 and the rotateAngle isn't modified
		 but if we rotate with touch, the distance of the moved finger will be added to the angle
		*/
		rotateAngle = previousAngle + (rotateDistance*100);

		// construct rotation matrix over (rad) angle
		if(currRotateAxis->direction == 0)
			currentRotation = Matrix4::rotationY(rad(rotateAngle));
		else if(currRotateAxis->direction == 1)
			currentRotation = Matrix4::rotationX(rad(rotateAngle));
		else if(currRotateAxis->direction == 2)
			currentRotation = Matrix4::rotationZ(rad(rotateAngle));
	}

	// PRINT STATES // FOR DEBUGGING
	//char text[512];
	//sprintf(text, "state: %s", (state == WAIT_FOR_INPUT_STATE ? "WAIT_FOR_INPUT_STATE" : (state == ROTATE_CUBE_STATE ? "ROTATE_CUBE_STATE" : (state == TOUCH_CUBE_STATE ? "TOUCH_CUBE_STATE" : "ROTATE_AXIS_STATE"))));
	//sceDbgFontPrint( 20, 0, 0xffffffff, (const SceChar8*) &text);
};



// ########################### GXM functions  ###########################

/* Initialize libgxm */
int initGxm( void )
{
/* ---------------------------------------------------------------------
	2. Initialize libgxm

	First we must initialize the libgxm library by calling sceGxmInitialize.
	The single argument to this function is the size of the parameter buffer to
	allocate for the GPU.  We will use the default 16MiB here.

	Once initialized, we need to create a rendering context to allow to us
	to render scenes on the GPU.  We use the default initialization
	parameters here to set the sizes of the various context ring buffers.

	Finally we create a render target to describe the geometry of the back
	buffers we will render to.  This object is used purely to schedule
	rendering jobs for the given dimensions, the color surface and
	depth/stencil surface must be allocated separately.
	--------------------------------------------------------------------- */

	int returnCode = SCE_OK;

	/* set up parameters */
	SceGxmInitializeParams initializeParams;
	memset( &initializeParams, 0, sizeof(SceGxmInitializeParams) );
	initializeParams.flags = 0;
	initializeParams.displayQueueMaxPendingCount = DISPLAY_MAX_PENDING_SWAPS;
	initializeParams.displayQueueCallback = displayCallback;
	initializeParams.displayQueueCallbackDataSize = sizeof(DisplayData);
	initializeParams.parameterBufferSize = SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE;

	/* start libgxm */
	returnCode = sceGxmInitialize( &initializeParams );
	SCE_DBG_ALWAYS_ASSERT( returnCode == SCE_OK );

	/* allocate ring buffer memory using default sizes */
	void *vdmRingBuffer = graphicsAlloc( SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE, SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE, 4, SCE_GXM_MEMORY_ATTRIB_READ, &s_vdmRingBufferUId );

	void *vertexRingBuffer = graphicsAlloc( SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE, SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE, 4, SCE_GXM_MEMORY_ATTRIB_READ, &s_vertexRingBufferUId );

	void *fragmentRingBuffer = graphicsAlloc( SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE, SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE, 4, SCE_GXM_MEMORY_ATTRIB_READ, &s_fragmentRingBufferUId );

	uint32_t fragmentUsseRingBufferOffset;
	void *fragmentUsseRingBuffer = fragmentUsseAlloc( SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE, &s_fragmentUsseRingBufferUId, &fragmentUsseRingBufferOffset );

	/* create a rendering context */
	memset( &s_contextParams, 0, sizeof(SceGxmContextParams) );
	s_contextParams.hostMem = malloc( SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE );
	s_contextParams.hostMemSize = SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE;
	s_contextParams.vdmRingBufferMem = vdmRingBuffer;
	s_contextParams.vdmRingBufferMemSize = SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE;
	s_contextParams.vertexRingBufferMem = vertexRingBuffer;
	s_contextParams.vertexRingBufferMemSize = SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE;
	s_contextParams.fragmentRingBufferMem = fragmentRingBuffer;
	s_contextParams.fragmentRingBufferMemSize = SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE;
	s_contextParams.fragmentUsseRingBufferMem = fragmentUsseRingBuffer;
	s_contextParams.fragmentUsseRingBufferMemSize = SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE;
	s_contextParams.fragmentUsseRingBufferOffset = fragmentUsseRingBufferOffset;
	returnCode = sceGxmCreateContext( &s_contextParams, &s_context );
	SCE_DBG_ALWAYS_ASSERT( returnCode == SCE_OK );

	/* set buffer sizes for this sample */
	const uint32_t patcherBufferSize = 64*1024;
	const uint32_t patcherVertexUsseSize = 64*1024;
	const uint32_t patcherFragmentUsseSize = 64*1024;

	/* allocate memory for buffers and USSE code */
	void *patcherBuffer = graphicsAlloc( SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE, patcherBufferSize, 4, SCE_GXM_MEMORY_ATTRIB_WRITE|SCE_GXM_MEMORY_ATTRIB_WRITE, &s_patcherBufferUId );

	uint32_t patcherVertexUsseOffset;
	void *patcherVertexUsse = vertexUsseAlloc( patcherVertexUsseSize, &s_patcherVertexUsseUId, &patcherVertexUsseOffset );

	uint32_t patcherFragmentUsseOffset;
	void *patcherFragmentUsse = fragmentUsseAlloc( patcherFragmentUsseSize, &s_patcherFragmentUsseUId, &patcherFragmentUsseOffset );

	/* create a shader patcher */
	SceGxmShaderPatcherParams patcherParams;
	memset( &patcherParams, 0, sizeof(SceGxmShaderPatcherParams) );
	patcherParams.userData = NULL;
	patcherParams.hostAllocCallback = &patcherHostAlloc;
	patcherParams.hostFreeCallback = &patcherHostFree;
	patcherParams.bufferAllocCallback = NULL;
	patcherParams.bufferFreeCallback = NULL;
	patcherParams.bufferMem = patcherBuffer;
	patcherParams.bufferMemSize = patcherBufferSize;
	patcherParams.vertexUsseAllocCallback = NULL;
	patcherParams.vertexUsseFreeCallback = NULL;
	patcherParams.vertexUsseMem = patcherVertexUsse;
	patcherParams.vertexUsseMemSize = patcherVertexUsseSize;
	patcherParams.vertexUsseOffset = patcherVertexUsseOffset;
	patcherParams.fragmentUsseAllocCallback = NULL;
	patcherParams.fragmentUsseFreeCallback = NULL;
	patcherParams.fragmentUsseMem = patcherFragmentUsse;
	patcherParams.fragmentUsseMemSize = patcherFragmentUsseSize;
	patcherParams.fragmentUsseOffset = patcherFragmentUsseOffset;
	returnCode = sceGxmShaderPatcherCreate( &patcherParams, &s_shaderPatcher );
	SCE_DBG_ALWAYS_ASSERT( returnCode == SCE_OK );

	/* create a render target */
	memset( &s_renderTargetParams, 0, sizeof(SceGxmRenderTargetParams) );
	s_renderTargetParams.flags = 0;
	s_renderTargetParams.width = DISPLAY_WIDTH;
	s_renderTargetParams.height = DISPLAY_HEIGHT;
	s_renderTargetParams.scenesPerFrame = 1;
	s_renderTargetParams.multisampleMode = SCE_GXM_MULTISAMPLE_NONE;
	s_renderTargetParams.multisampleLocations	= 0;
	s_renderTargetParams.driverMemBlock = SCE_UID_INVALID_UID;

	returnCode = sceGxmCreateRenderTarget( &s_renderTargetParams, &s_renderTarget );
	SCE_DBG_ALWAYS_ASSERT( returnCode == SCE_OK );


/* ---------------------------------------------------------------------
	3. Allocate display buffers, set up the display queue

	We will allocate our back buffers in CDRAM, and create a color
	surface for each of them.

	To allow display operations done by the CPU to be synchronized with
	rendering done by the GPU, we also create a SceGxmSyncObject for each
	display buffer.  This sync object will be used with each scene that
	renders to that buffer and when queueing display flips that involve
	that buffer (either flipping from or to).

	Finally we create a display queue object that points to our callback
	function.
	--------------------------------------------------------------------- */

	/* allocate memory and sync objects for display buffers */
	for ( unsigned int i = 0 ; i < DISPLAY_BUFFER_COUNT ; ++i )
	{
		/* allocate memory with large size to ensure physical contiguity */
		s_displayBufferData[i] = graphicsAlloc( SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RWDATA, ALIGN(4*DISPLAY_STRIDE_IN_PIXELS*DISPLAY_HEIGHT, 1*1024*1024), SCE_GXM_COLOR_SURFACE_ALIGNMENT, SCE_GXM_MEMORY_ATTRIB_READ|SCE_GXM_MEMORY_ATTRIB_WRITE, &s_displayBufferUId[i] );
		SCE_DBG_ALWAYS_ASSERT( s_displayBufferData[i] );

		/* memset the buffer to debug color */
		for ( unsigned int y = 0 ; y < DISPLAY_HEIGHT ; ++y )
		{
			unsigned int *row = (unsigned int *)s_displayBufferData[i] + y*DISPLAY_STRIDE_IN_PIXELS;

			for ( unsigned int x = 0 ; x < DISPLAY_WIDTH ; ++x )
			{
				row[x] = 0x0;
			}
		}

		/* initialize a color surface for this display buffer */
		returnCode = sceGxmColorSurfaceInit( &s_displaySurface[i], DISPLAY_COLOR_FORMAT, SCE_GXM_COLOR_SURFACE_LINEAR, SCE_GXM_COLOR_SURFACE_SCALE_NONE,
											 SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_STRIDE_IN_PIXELS, s_displayBufferData[i] );
		SCE_DBG_ALWAYS_ASSERT( returnCode == SCE_OK );

		/* create a sync object that we will associate with this buffer */
		returnCode = sceGxmSyncObjectCreate( &s_displayBufferSync[i] );
		SCE_DBG_ALWAYS_ASSERT( returnCode == SCE_OK );
	}

	/* compute the memory footprint of the depth buffer */
	const uint32_t alignedWidth = ALIGN( DISPLAY_WIDTH, SCE_GXM_TILE_SIZEX );
	const uint32_t alignedHeight = ALIGN( DISPLAY_HEIGHT, SCE_GXM_TILE_SIZEY );
	uint32_t sampleCount = alignedWidth*alignedHeight;
	uint32_t depthStrideInSamples = alignedWidth;

	/* allocate it */
	void *depthBufferData = graphicsAlloc( SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE, 4*sampleCount, SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT, SCE_GXM_MEMORY_ATTRIB_READ|SCE_GXM_MEMORY_ATTRIB_WRITE, &s_depthBufferUId );

	/* create the SceGxmDepthStencilSurface structure */
	returnCode = sceGxmDepthStencilSurfaceInit( &s_depthSurface, SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24, SCE_GXM_DEPTH_STENCIL_SURFACE_TILED, depthStrideInSamples, depthBufferData, NULL );
	SCE_DBG_ALWAYS_ASSERT( returnCode == SCE_OK );

	return returnCode;
}

/* Create libgxm scenes */
void createGxmData( void )
{
/* ---------------------------------------------------------------------
	4. Create a shader patcher and register programs

	A shader patcher object is required to produce vertex and fragment
	programs from the shader compiler output.  First we create a shader
	patcher instance, using callback functions to allow it to allocate
	and free host memory for internal state.

	In order to create vertex and fragment programs for a particular
	shader, the compiler output must first be registered to obtain an ID
	for that shader.  Within a single ID, vertex and fragment programs
	are reference counted and could be shared if created with identical
	parameters.  To maximise this sharing, programs should only be
	registered with the shader patcher once if possible, so we will do
	this now.
	--------------------------------------------------------------------- */

	/* register programs with the patcher */
	int returnCode = sceGxmShaderPatcherRegisterProgram( s_shaderPatcher, &binaryClearVGxpStart, &s_clearVertexProgramId );
	SCE_DBG_ALWAYS_ASSERT( returnCode == SCE_OK );
	returnCode = sceGxmShaderPatcherRegisterProgram( s_shaderPatcher, &binaryClearFGxpStart, &s_clearFragmentProgramId );
	SCE_DBG_ALWAYS_ASSERT( returnCode == SCE_OK );

    returnCode = sceGxmShaderPatcherRegisterProgram( s_shaderPatcher, &binaryBasicVGxpStart, &s_basicVertexProgramId );
	SCE_DBG_ALWAYS_ASSERT( returnCode == SCE_OK );
	returnCode = sceGxmShaderPatcherRegisterProgram( s_shaderPatcher, &binaryBasicFGxpStart, &s_basicFragmentProgramId );
    SCE_DBG_ALWAYS_ASSERT( returnCode == SCE_OK );


/* ---------------------------------------------------------------------
	5. Create the programs and data for the clear

	On SGX hardware, vertex programs must perform the unpack operations
	on vertex data, so we must define our vertex formats in order to
	create the vertex program.  Similarly, fragment programs must be
	specialized based on how they output their pixels and MSAA mode
	(and texture format on ES1).

	We define the clear geometry vertex format here and create the vertex
	and fragment program.

	The clear vertex and index data is static, we allocate and write the
	data here.
	--------------------------------------------------------------------- */

	/* get attributes by name to create vertex format bindings */
	const SceGxmProgram *clearProgram = sceGxmShaderPatcherGetProgramFromId( s_clearVertexProgramId );
	SCE_DBG_ALWAYS_ASSERT( clearProgram );
	const SceGxmProgramParameter *paramClearPositionAttribute = sceGxmProgramFindParameterByName( clearProgram, "aPosition" );
	SCE_DBG_ALWAYS_ASSERT( paramClearPositionAttribute && ( sceGxmProgramParameterGetCategory(paramClearPositionAttribute) == SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE ) );

	/* create clear vertex format */
	SceGxmVertexAttribute clearVertexAttributes[1];
	SceGxmVertexStream clearVertexStreams[1];
	clearVertexAttributes[0].streamIndex = 0;
	clearVertexAttributes[0].offset = 0;
	clearVertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	clearVertexAttributes[0].componentCount = 2;
	clearVertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex( paramClearPositionAttribute );
	clearVertexStreams[0].stride = sizeof(ClearVertex);
	clearVertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

	/* create clear programs */
	returnCode = sceGxmShaderPatcherCreateVertexProgram( s_shaderPatcher, s_clearVertexProgramId, clearVertexAttributes, 1, clearVertexStreams, 1, &s_clearVertexProgram );
	SCE_DBG_ALWAYS_ASSERT( returnCode == SCE_OK );

	returnCode = sceGxmShaderPatcherCreateFragmentProgram( s_shaderPatcher, s_clearFragmentProgramId,
														   SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4, SCE_GXM_MULTISAMPLE_NONE, NULL,
														   sceGxmShaderPatcherGetProgramFromId(s_clearVertexProgramId), &s_clearFragmentProgram );
	SCE_DBG_ALWAYS_ASSERT( returnCode == SCE_OK );

	/* create the clear triangle vertex/index data */
	s_clearVertices = (ClearVertex *)graphicsAlloc( SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE, 3*sizeof(ClearVertex), 4, SCE_GXM_MEMORY_ATTRIB_READ, &s_clearVerticesUId );
	s_clearIndices = (uint16_t *)graphicsAlloc( SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE, 3*sizeof(uint16_t), 2, SCE_GXM_MEMORY_ATTRIB_READ, &s_clearIndicesUId );

	s_clearVertices[0].x = -1.0f;
	s_clearVertices[0].y = -1.0f;
	s_clearVertices[1].x =  3.0f;
	s_clearVertices[1].y = -1.0f;
	s_clearVertices[2].x = -1.0f;
	s_clearVertices[2].y =  3.0f;

	s_clearIndices[0] = 0;
	s_clearIndices[1] = 1;
	s_clearIndices[2] = 2;

   

	/* ---------------------------------------------------------------------
		CREATE BASIC VERTICES
		--------------------------------------------------------------------- */
    /* get attributes by name to create vertex format bindings */
	/* first retrieve the underlying program to extract binding information */
	const SceGxmProgram *basicProgram = sceGxmShaderPatcherGetProgramFromId( s_basicVertexProgramId );
	SCE_DBG_ALWAYS_ASSERT( basicProgram );
	// position attribute of vertex
	const SceGxmProgramParameter *paramBasicPositionAttribute = sceGxmProgramFindParameterByName( basicProgram, "aPosition" );
	SCE_DBG_ALWAYS_ASSERT( paramBasicPositionAttribute && ( sceGxmProgramParameterGetCategory(paramBasicPositionAttribute) == SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE ) );
	// normal attribute of vertex
	const SceGxmProgramParameter *paramBasicNormalAttribute = sceGxmProgramFindParameterByName( basicProgram, "aNormal" );
	SCE_DBG_ALWAYS_ASSERT( paramBasicNormalAttribute && ( sceGxmProgramParameterGetCategory(paramBasicNormalAttribute) == SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE ) );
	// color attribute of vertex
	const SceGxmProgramParameter *paramBasicColorAttribute = sceGxmProgramFindParameterByName( basicProgram, "aColor" );
	SCE_DBG_ALWAYS_ASSERT( paramBasicColorAttribute && ( sceGxmProgramParameterGetCategory(paramBasicColorAttribute) == SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE ) );
	// rotate attribute of vertex (if the vertex should use the uniform param. rotation matrix)
	const SceGxmProgramParameter *paramBasicRotateAttribute = sceGxmProgramFindParameterByName( basicProgram, "aRotate" );
	SCE_DBG_ALWAYS_ASSERT( paramBasicRotateAttribute && ( sceGxmProgramParameterGetCategory(paramBasicRotateAttribute) == SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE ) );


	/* create shaded triangle vertex format */
	SceGxmVertexAttribute basicVertexAttributes[4];
	SceGxmVertexStream basicVertexStreams[1];
	
	// attribute positition: 12
	basicVertexAttributes[0].streamIndex = 0;
	basicVertexAttributes[0].offset = 0;
	basicVertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	basicVertexAttributes[0].componentCount = 3;
	basicVertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex( paramBasicPositionAttribute );
	// attribute normal: 12
	basicVertexAttributes[1].streamIndex = 0;
	basicVertexAttributes[1].offset = 12;
	basicVertexAttributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_F32; 
	basicVertexAttributes[1].componentCount = 3;
	basicVertexAttributes[1].regIndex = sceGxmProgramParameterGetResourceIndex( paramBasicNormalAttribute );
	// attribute color: 4
	basicVertexAttributes[2].streamIndex = 0;
	basicVertexAttributes[2].offset = 24;
	basicVertexAttributes[2].format = SCE_GXM_ATTRIBUTE_FORMAT_U8N;
	basicVertexAttributes[2].componentCount = 4;
	basicVertexAttributes[2].regIndex = sceGxmProgramParameterGetResourceIndex( paramBasicColorAttribute );
	// attribute rotation: 1
	basicVertexAttributes[3].streamIndex = 0;
	basicVertexAttributes[3].offset = 28;
	basicVertexAttributes[3].format = SCE_GXM_ATTRIBUTE_FORMAT_U8N;
	basicVertexAttributes[3].componentCount = 1;
	basicVertexAttributes[3].regIndex = sceGxmProgramParameterGetResourceIndex( paramBasicRotateAttribute );
	
	// vertex stream
	basicVertexStreams[0].stride = sizeof(BasicVertex);
	basicVertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

	/* create shaded triangle shaders */
	// create vertex shader
	returnCode = sceGxmShaderPatcherCreateVertexProgram( s_shaderPatcher, s_basicVertexProgramId, basicVertexAttributes, 4,
														 basicVertexStreams, 1, &s_basicVertexProgram );
	SCE_DBG_ALWAYS_ASSERT( returnCode == SCE_OK );
	// create fragment shader
	returnCode = sceGxmShaderPatcherCreateFragmentProgram( s_shaderPatcher, s_basicFragmentProgramId,
														   SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4, SCE_GXM_MULTISAMPLE_NONE, NULL,
														   sceGxmShaderPatcherGetProgramFromId(s_basicVertexProgramId), &s_basicFragmentProgram );
	SCE_DBG_ALWAYS_ASSERT( returnCode == SCE_OK );

	/* find vertex uniforms by name and cache parameter information */
	// world view position / world coordinates matrix
	s_wvpParam = sceGxmProgramFindParameterByName( basicProgram, "wvp" );
	SCE_DBG_ALWAYS_ASSERT( s_wvpParam && ( sceGxmProgramParameterGetCategory( s_wvpParam ) == SCE_GXM_PARAMETER_CATEGORY_UNIFORM ) );
	// cube rotation / object coordinates matrix
	s_rotParam = sceGxmProgramFindParameterByName( basicProgram, "rot" );
	SCE_DBG_ALWAYS_ASSERT( s_rotParam && ( sceGxmProgramParameterGetCategory( s_rotParam ) == SCE_GXM_PARAMETER_CATEGORY_UNIFORM ) );
	// axis rotation / additional matrix for all rotating vertices 
	s_rotateParam = sceGxmProgramFindParameterByName( basicProgram, "rotate" );
	SCE_DBG_ALWAYS_ASSERT( s_rotateParam && ( sceGxmProgramParameterGetCategory( s_rotateParam ) == SCE_GXM_PARAMETER_CATEGORY_UNIFORM ) );

	/* create shaded triangle vertex/index data */
	// vertices 216 (4 vertices per cube face)
	s_basicVertices = (BasicVertex *)graphicsAlloc( SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE, 4*6*9*sizeof(BasicVertex), 4, SCE_GXM_MEMORY_ATTRIB_READ, &s_basicVerticesUId );
	// indices 324 (6 indices per cube face)
	s_basicIndices = (uint16_t *)graphicsAlloc( SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE, 6*6*9*sizeof(uint16_t), 2, SCE_GXM_MEMORY_ATTRIB_READ, &s_basicIndiceUId );
	

	// ---------------- cube sides --------------------
	// vertex counter
	int count = 0;
	// side counter
	int sideIndex = 0;
	// color of first cube side
	Color color = RED;
	// loop 0,1,2 for directions x,y,z
	for(int type = 0; type < 3; ++type)
	{
		// loop -1,+1 for offsets; 2 sides on every axis
		for(int dir = -1; dir < 2; dir+=2)
		{
			// create the side
			CreateCubeSide(&(s_basicVertices[count]), type, dir, color, sideIndex++);
			// the next side is 4 vertices * 9 faces after the current
			count += (4*9);
			// change color for the next side
			if(color == RED)			color = YELLOW;
			else if(color == YELLOW)	color = GREEN;
			else if(color == GREEN)		color = ORANGE;
			else if(color == ORANGE)	color = WHITE;
			else						color = BLUE;
		}
	}
	
	// ---------------- indices --------------------
	// re-use the counter; reset
	count = 0;
	// vertex counter
	int baseIndex = 0;
	// loop 6 indices per face * 9 faces
	for(int i = 0; i < 6*9; ++i)
	{
		// first triangle
		s_basicIndices[count++] = baseIndex;
		s_basicIndices[count++] = baseIndex+1;
		s_basicIndices[count++] = baseIndex+2;
		// second triangle
		s_basicIndices[count++] = baseIndex;
		s_basicIndices[count++] = baseIndex+3;
		s_basicIndices[count++] = baseIndex+2;
		// increment vertex counter (only 4 vertices for 1 face)
		baseIndex += 4;
	}
	
	// ------------------- cube axis -----------------
	// add the cube faces to the cube axes
	// ---------------------- X ----------------------
	// X-AXIS [0]
	addToCubeAxis(&(axis[0][0]), 0, FRONT, 6,7,8);
	addToCubeAxis(&(axis[0][0]), 3, RIGHT, 8,5,2);
	addToCubeAxis(&(axis[0][0]), 6, BACK,  8,7,6);
	addToCubeAxis(&(axis[0][0]), 9, LEFT,  2,5,8);	
	// X-AXIS [1]
	addToCubeAxis(&(axis[0][1]), 0, FRONT, 3,4,5);
	addToCubeAxis(&(axis[0][1]), 3, RIGHT, 7,4,1);
	addToCubeAxis(&(axis[0][1]), 6, BACK,  5,4,3);
	addToCubeAxis(&(axis[0][1]), 9, LEFT,  1,4,7);	
	// X-AXIS [2]
	addToCubeAxis(&(axis[0][2]), 0, FRONT, 0,1,2);
	addToCubeAxis(&(axis[0][2]), 3, RIGHT, 6,3,0);
	addToCubeAxis(&(axis[0][2]), 6, BACK,  2,1,0);
	addToCubeAxis(&(axis[0][2]), 9, LEFT,  0,3,6);	

	// ---------------------- Y ----------------------
	// Y-AXIS [0]
	addToCubeAxis(&(axis[1][0]), 0, FRONT, 6,3,0);
	addToCubeAxis(&(axis[1][0]), 3, DOWN,  2,1,0);
	addToCubeAxis(&(axis[1][0]), 6, BACK,  0,3,6);
	addToCubeAxis(&(axis[1][0]), 9, TOP,   0,1,2);	
	// Y-AXIS [1]
	addToCubeAxis(&(axis[1][1]), 0, FRONT, 7,4,1);
	addToCubeAxis(&(axis[1][1]), 3, DOWN,  5,4,3);
	addToCubeAxis(&(axis[1][1]), 6, BACK,  1,4,7);
	addToCubeAxis(&(axis[1][1]), 9, TOP,   3,4,5);	
	// Y-AXIS [2]
	addToCubeAxis(&(axis[1][2]), 0, FRONT, 8,5,2);
	addToCubeAxis(&(axis[1][2]), 3, DOWN,  8,7,6);
	addToCubeAxis(&(axis[1][2]), 6, BACK,  2,5,8);
	addToCubeAxis(&(axis[1][2]), 9, TOP,   6,7,8);	

	// ---------------------- Z ----------------------
	// Z-AXIS [0]
	addToCubeAxis(&(axis[2][0]), 0, LEFT,  2,1,0);
	addToCubeAxis(&(axis[2][0]), 3, DOWN,  0,3,6);
	addToCubeAxis(&(axis[2][0]), 6, RIGHT, 0,1,2);
	addToCubeAxis(&(axis[2][0]), 9, TOP,   6,3,0);	
	// Z-AXIS [1]
	addToCubeAxis(&(axis[2][1]), 0, LEFT,  5,4,3);
	addToCubeAxis(&(axis[2][1]), 3, DOWN,  1,4,7);
	addToCubeAxis(&(axis[2][1]), 6, RIGHT, 3,4,5);
	addToCubeAxis(&(axis[2][1]), 9, TOP,   7,4,1);	
	// Z-AXIS [2]		 
	addToCubeAxis(&(axis[2][2]), 0, LEFT,  8,7,6);
	addToCubeAxis(&(axis[2][2]), 3, DOWN,  2,5,8);
	addToCubeAxis(&(axis[2][2]), 6, RIGHT, 6,7,8);
	addToCubeAxis(&(axis[2][2]), 9, TOP,   8,5,2);


	// set the cube side pointer of the axes to the bordered cube side
	// 3 axes
	for(int i = 0; i < 3; ++i)
	{
		// with 3 rows
		for(int j = 0; j < 3; ++j) {
			// preset borderSideIndex for axes with no attached side
			int borderSideIndex = -1;
			// tell the axis its direction
			axis[i][j].direction = i;
			// axis[x][0] borders TOP and axis[x][2] borders DOWN side ...
			if(i == 0) {
				if(j == 0)
					borderSideIndex = TOP;
				else if(j == 2)
					borderSideIndex = DOWN;
			} else if(i == 1) {
				if(j == 0)
					borderSideIndex = LEFT;
				else if(j == 2)
					borderSideIndex = RIGHT;
			} else {
				if(j == 0)
					borderSideIndex = BACK;
				else if(j == 2)
					borderSideIndex = FRONT;
			}
			// set the pointer to the side or to NULL if the axis is in the center
			if(borderSideIndex >= 0)
				axis[i][j].borderSide = &sides[borderSideIndex];
			else
				axis[i][j].borderSide = NULL;
		}
	}

	//  hightlight current face (initialized on declaration)
	currentFace->setCurrent(true);
}


// ########################### CUBE INIT  ###########################

// adds 3 faces of a cube side to an axis
// a = pointer to axis, startIndex = position of face on axis, index1-3 = indices of faces on the cube side
void addToCubeAxis(CubeAxis* a, int startIndex, int sideIndex, int index1, int index2, int index3)
{
	// get first face index
	int currIndex = index1;
	// loop over all 3 faces
	for(int i = 0; i < 3; ++i)
	{
		// get the face
		CubeFace* face = &(sides[sideIndex].faces[currIndex]);

		// if the axis1 pointer of the face isn't set; let it point to this axis
		// or else -> take the axis2 slot
		if(face->axis1 == NULL) {
			face->axis1 = a;
			face->axis1Pos = startIndex;
		} else {
			face->axis2 = a;
			face->axis2Pos = startIndex;
		}
		// add the face to the axis
		a->faces[startIndex++] = face;
		// get the next index
		currIndex = (i == 0 ? index2 : index3);
	}
}

/* create the cube side with 9 faces
   > field		= start pointer of the vertex array
   > type		= axis x/y/z -> 0/1/2
   > direction	= offset from origin -> -1 / +1
   > color		= color of the cube side
   > sideIndex	= index of the created cube side 
*/
void CreateCubeSide(BasicVertex* field, int type, int direction, Color color, int sideIndex)
{
	// get a pointer to the space where we want to create the side
	CubeSide* side = &(sides[sideIndex]);

	// set the offset of the vertices coordinate axis (-0.6 / +0.6)
	for(int i = 0; i < 4*9; ++i)
		field[i].position[type] = direction * 0.6f;

	// get the direction code of the other 2 axes and set them as loxal x/y dim
	int localXDim = (type + 1) % 3;
	int localYDim = (type + 2) % 3;
	
	// create 2 vectors for the dimenstions (x -> 0 = (1,0,0), y -> 1 = (0,1,0), z -> 2 = (0,0,1))
	side->localXDim = Vector3(localXDim == 0 ? 1 : 0, localXDim == 1 ? 1: 0, localXDim == 2 ? 1 : 0);
	side->localYDim = Vector3(localYDim == 0 ? 1 : 0, localYDim == 1 ? 1: 0, localYDim == 2 ? 1 : 0);

	// vertex counter
	int count = 0;
	// face counter
	int faceIndex = 0;
	// space between faces
	float offset = 0.01f;
	
	// our cube side goes from -0.6 to 0.6 -> width/height: 1.2 
	// loop over 3 cols
	for(int i = 0; i < 3; i++)
	{
		// loop over 3 rows
		for(int j = 0; j < 3; j++)
		{
			// set local-x-position of the vertices
			// vertices on the left
			field[count++].position[localXDim] = -0.6f + offset + (j*0.4f);
			field[count++].position[localXDim] = -0.6f + offset + (j*0.4f);
			// vertices on the right
			field[count++].position[localXDim] = -0.6f + offset + (j*0.4f) + 0.4f - 2*offset;
			field[count++].position[localXDim] = -0.6f + offset + (j*0.4f) + 0.4f - 2*offset;
			// decrement vertex counter because we have to run over them again
			count -= 4;

			// set local-y-position of the vertives
			field[count++].position[localYDim] = -0.6f + offset + (i*0.4f) + 0.4f - 2*offset;
			field[count++].position[localYDim] = -0.6f + offset + (i*0.4f);
			field[count++].position[localYDim] = -0.6f + offset + (i*0.4f);
			field[count++].position[localYDim] = -0.6f + offset + (i*0.4f) + 0.4f - 2*offset;	
			// decrement vertex counter because we have to run over them again	
			count -= 4;

			// add the transformed vertices to the cube side object
			for(int k = 0; k < 4; ++k) 
				side->faces[faceIndex].vertices[k] = &field[count++];
			
			// increment the face counter
			faceIndex ++;
		}
	}


	// COLOR & AXIS POINTER
	count = 0;
	faceIndex = 0;
	for(int i = 0; i < 9; ++i) {
		side->faces[faceIndex].axis1 = NULL;
		side->faces[faceIndex].axis2 = NULL;
		side->faces[faceIndex++].setColor(color);
	}

	// NORMALS & ORIGIN
	// construct the normal of the cube side
	float normal[3];
	normal[0] = normal[1] = normal[2] = 0.f;
	normal[type] = direction;
	// construct a vector for the normal
	side->normal = Vector3(normal[0], normal[1], normal[2]);
	// construct the origin vector of the side 
	side->origin = Vector3(normal[0]*0.6f, normal[1]*0.6f, normal[2]*0.6f);

	// for every 4 vertices on the 9 faces of the side
	for(int i = 0; i < 4*9; ++i)
	{
		// and every axis
		for(int j = 0; j < 3; ++j)
		{
			// set the normal of the vertex for later use in the vertex shader
			field[i].normal[j] = normal[j];
		}
	}
}



// ########################### RENDER  ###########################

/* Main render function */
void render( void )
{
	/* render libgxm scenes */
	renderGxm();
}

/* render gxm scenes */
void renderGxm( void )
{
/* -----------------------------------------------------------------
	8. Rendering step

	This sample renders a single scene containing the clear triangle.
	Before any drawing can take place, a scene must be started.
	We render to the back buffer, so it is also important to use a
	sync object to ensure that these rendering operations are
	synchronized with display operations.

	The clear triangle shaders do not declare any uniform variables,
	so this may be rendered immediately after setting the vertex and
	fragment program.

	Once clear triangle have been drawn the scene can be ended, which
	submits it for rendering on the GPU.
	----------------------------------------------------------------- */

	/* start rendering to the render target */
	sceGxmBeginScene( s_context, 0, s_renderTarget, NULL, NULL, s_displayBufferSync[s_displayBackBufferIndex], &s_displaySurface[s_displayBackBufferIndex], &s_depthSurface );

	/* set clear shaders */
	sceGxmSetVertexProgram( s_context, s_clearVertexProgram );
	sceGxmSetFragmentProgram( s_context, s_clearFragmentProgram );

	/* draw ther clear triangle */
	sceGxmSetVertexStream( s_context, 0, s_clearVertices );
	sceGxmDraw( s_context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, s_clearIndices, 3 );

    /* render the  triangle */
	sceGxmSetVertexProgram( s_context, s_basicVertexProgram );
	sceGxmSetFragmentProgram( s_context, s_basicFragmentProgram );

	/* set the vertex program constants / uniform paramteres */
	void *vertexDefaultBuffer;
	sceGxmReserveVertexDefaultUniformBuffer( s_context, &vertexDefaultBuffer );
	// world space matrix
	sceGxmSetUniformDataF( vertexDefaultBuffer, s_wvpParam,	   0, 16, (float*)&finalTransformation ); 
	// object space matrix
	sceGxmSetUniformDataF( vertexDefaultBuffer, s_rotParam,	   0, 16, (float*)&finalRotation );
	// additional rotation matrix for rotating vertices
	sceGxmSetUniformDataF( vertexDefaultBuffer, s_rotateParam, 0, 16, (float*)&currentRotation );
	

	/* draw the triangles */
	sceGxmSetVertexStream( s_context, 0, s_basicVertices );								
	sceGxmDraw( s_context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, s_basicIndices, 6*6*9 );

	/* stop rendering to the render target */
	sceGxmEndScene( s_context, NULL, NULL );
}



/* queue a display swap and cycle our buffers */
void cycleDisplayBuffers( void )
{
/* -----------------------------------------------------------------
	9-a. Flip operation

	Now we have finished submitting rendering work for this frame it
	is time to submit a flip operation.  As part of specifying this
	flip operation we must provide the sync objects for both the old
	buffer and the new buffer.  This is to allow synchronization both
	ways: to not flip until rendering is complete, but also to ensure
	that future rendering to these buffers does not start until the
	flip operation is complete.

	Once we have queued our flip, we manually cycle through our back
	buffers before starting the next frame.
	----------------------------------------------------------------- */

	/* PA heartbeat to notify end of frame */
	sceGxmPadHeartbeat( &s_displaySurface[s_displayBackBufferIndex], s_displayBufferSync[s_displayBackBufferIndex] );

	/* queue the display swap for this frame */
	DisplayData displayData;
	displayData.address = s_displayBufferData[s_displayBackBufferIndex];

	/* front buffer is OLD buffer, back buffer is NEW buffer */
	sceGxmDisplayQueueAddEntry( s_displayBufferSync[s_displayFrontBufferIndex], s_displayBufferSync[s_displayBackBufferIndex], &displayData );

	/* update buffer indices */
	s_displayFrontBufferIndex = s_displayBackBufferIndex;
	s_displayBackBufferIndex = (s_displayBackBufferIndex + 1) % DISPLAY_BUFFER_COUNT;
}



// ########################### DESTROY & FINALIZE  ###########################

/* Destroy Gxm Data */
void destroyGxmData( void )
{
/* ---------------------------------------------------------------------
	11. Destroy the programs and data for the clear and spinning triangle

	Once the GPU is finished, we release all our programs.
	--------------------------------------------------------------------- */

	/* clean up allocations */
	sceGxmShaderPatcherReleaseFragmentProgram( s_shaderPatcher, s_clearFragmentProgram );
	sceGxmShaderPatcherReleaseVertexProgram( s_shaderPatcher, s_clearVertexProgram );
	graphicsFree( s_clearIndicesUId );
	graphicsFree( s_clearVerticesUId );

	/* wait until display queue is finished before deallocating display buffers */
	sceGxmDisplayQueueFinish();

	/* unregister programs and destroy shader patcher */
	sceGxmShaderPatcherUnregisterProgram( s_shaderPatcher, s_clearFragmentProgramId );
	sceGxmShaderPatcherUnregisterProgram( s_shaderPatcher, s_clearVertexProgramId );
	sceGxmShaderPatcherDestroy( s_shaderPatcher );
	fragmentUsseFree( s_patcherFragmentUsseUId );
	vertexUsseFree( s_patcherVertexUsseUId );
	graphicsFree( s_patcherBufferUId );
}



/* ShutDown libgxm */
int shutdownGxm( void )
{
/* ---------------------------------------------------------------------
	12. Finalize libgxm

	Once the GPU is finished, we deallocate all our memory,
	destroy all object and finally terminate libgxm.
	--------------------------------------------------------------------- */

	int returnCode = SCE_OK;

	graphicsFree( s_depthBufferUId );

	for ( unsigned int i = 0 ; i < DISPLAY_BUFFER_COUNT; ++i )
	{
		memset( s_displayBufferData[i], 0, DISPLAY_HEIGHT*DISPLAY_STRIDE_IN_PIXELS*4 );
		graphicsFree( s_displayBufferUId[i] );
		sceGxmSyncObjectDestroy( s_displayBufferSync[i] );
	}

	/* destroy the render target */
	sceGxmDestroyRenderTarget( s_renderTarget );

	/* destroy the context */
	sceGxmDestroyContext( s_context );

	fragmentUsseFree( s_fragmentUsseRingBufferUId );
	graphicsFree( s_fragmentRingBufferUId );
	graphicsFree( s_vertexRingBufferUId );
	graphicsFree( s_vdmRingBufferUId );
	free( s_contextParams.hostMem );

	/* terminate libgxm */
	sceGxmTerminate();

	return returnCode;
}



// ########################### MEMORY FUNCTIONS  ###########################

/* Host alloc */
static void *patcherHostAlloc( void *userData, unsigned int size )
{
	(void)( userData );

	return malloc( size );
}

/* Host free */
static void patcherHostFree( void *userData, void *mem )
{
	(void)( userData );

	free( mem );
}

/* Display callback */
void displayCallback( const void *callbackData )
{
/* -----------------------------------------------------------------
	10-b. Flip operation

	The callback function will be called from an internal thread once
	queued GPU operations involving the sync objects is complete.
	Assuming we have not reached our maximum number of queued frames,
	this function returns immediately.
	----------------------------------------------------------------- */

	SceDisplayFrameBuf framebuf;

	/* cast the parameters back */
	const DisplayData *displayData = (const DisplayData *)callbackData;


    // Render debug text.
    /* set framebuffer info */
	SceDbgFontFrameBufInfo info;
	memset( &info, 0, sizeof(SceDbgFontFrameBufInfo) );
	info.frameBufAddr = (SceUChar8 *)displayData->address;
	info.frameBufPitch = DISPLAY_STRIDE_IN_PIXELS;
	info.frameBufWidth = DISPLAY_WIDTH;
	info.frameBufHeight = DISPLAY_HEIGHT;
	info.frameBufPixelformat = DBGFONT_PIXEL_FORMAT;

	/* flush font buffer */
	int returnCode = sceDbgFontFlush( &info );
	SCE_DBG_ALWAYS_ASSERT( returnCode == SCE_OK );
	

	/* wwap to the new buffer on the next VSYNC */
	memset(&framebuf, 0x00, sizeof(SceDisplayFrameBuf));
	framebuf.size        = sizeof(SceDisplayFrameBuf);
	framebuf.base        = displayData->address;
	framebuf.pitch       = DISPLAY_STRIDE_IN_PIXELS;
	framebuf.pixelformat = DISPLAY_PIXEL_FORMAT;
	framebuf.width       = DISPLAY_WIDTH;
	framebuf.height      = DISPLAY_HEIGHT;
	returnCode = sceDisplaySetFrameBuf( &framebuf, SCE_DISPLAY_UPDATETIMING_NEXTVSYNC );
	SCE_DBG_ALWAYS_ASSERT( returnCode == SCE_OK );

	/* block this callback until the swap has occurred and the old buffer is no longer displayed */
	returnCode = sceDisplayWaitVblankStart();
	SCE_DBG_ALWAYS_ASSERT( returnCode == SCE_OK );
}

/* Alloc used by libgxm */
static void *graphicsAlloc( SceKernelMemBlockType type, uint32_t size, uint32_t alignment, uint32_t attribs, SceUID *uid )
{
/*	Since we are using sceKernelAllocMemBlock directly, we cannot directly
	use the alignment parameter.  Instead, we must allocate the size to the
	minimum for this memblock type, and just SCE_DBG_ALWAYS_ASSERT that this will cover
	our desired alignment.

	Developers using their own heaps should be able to use the alignment
	parameter directly for more minimal padding.
*/

	if( type == SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RWDATA )
	{
		/* CDRAM memblocks must be 256KiB aligned */
		SCE_DBG_ALWAYS_ASSERT( alignment <= 256*1024 );
		size = ALIGN( size, 256*1024 );
	}
	else
	{
		/* LPDDR memblocks must be 4KiB aligned */
		SCE_DBG_ALWAYS_ASSERT( alignment <= 4*1024 );
		size = ALIGN( size, 4*1024 );
	}

	/* allocate some memory */
	*uid = sceKernelAllocMemBlock( "simple", type, size, NULL );
	SCE_DBG_ALWAYS_ASSERT( *uid >= SCE_OK );

	/* grab the base address */
	void *mem = NULL;
	int err = sceKernelGetMemBlockBase( *uid, &mem );
	SCE_DBG_ALWAYS_ASSERT( err == SCE_OK );

	/* map for the GPU */
	err = sceGxmMapMemory( mem, size, attribs );
	SCE_DBG_ALWAYS_ASSERT( err == SCE_OK );

	/* done */
	return mem;
}

/* Free used by libgxm */
static void graphicsFree( SceUID uid )
{
	/* grab the base address */
	void *mem = NULL;
	int err = sceKernelGetMemBlockBase(uid, &mem);
	SCE_DBG_ALWAYS_ASSERT(err == SCE_OK);

	// unmap memory
	err = sceGxmUnmapMemory(mem);
	SCE_DBG_ALWAYS_ASSERT(err == SCE_OK);

	// free the memory block
	err = sceKernelFreeMemBlock(uid);
	SCE_DBG_ALWAYS_ASSERT(err == SCE_OK);
}

/* vertex alloc used by libgxm */
static void *vertexUsseAlloc( uint32_t size, SceUID *uid, uint32_t *usseOffset )
{
	/* align to memblock alignment for LPDDR */
	size = ALIGN( size, 4096 );

	/* allocate some memory */
	*uid = sceKernelAllocMemBlock( "simple", SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE, size, NULL );
	SCE_DBG_ALWAYS_ASSERT( *uid >= SCE_OK );

	/* grab the base address */
	void *mem = NULL;
	int err = sceKernelGetMemBlockBase( *uid, &mem );
	SCE_DBG_ALWAYS_ASSERT( err == SCE_OK );

	/* map as vertex USSE code for the GPU */
	err = sceGxmMapVertexUsseMemory( mem, size, usseOffset );
	SCE_DBG_ALWAYS_ASSERT( err == SCE_OK );

	return mem;
}

/* vertex free used by libgxm */
static void vertexUsseFree( SceUID uid )
{
	/* grab the base address */
	void *mem = NULL;
	int err = sceKernelGetMemBlockBase( uid, &mem );
	SCE_DBG_ALWAYS_ASSERT(err == SCE_OK);

	/* unmap memory */
	err = sceGxmUnmapVertexUsseMemory( mem );
	SCE_DBG_ALWAYS_ASSERT(err == SCE_OK);

	/* free the memory block */
	err = sceKernelFreeMemBlock( uid );
	SCE_DBG_ALWAYS_ASSERT( err == SCE_OK );
}

/* fragment alloc used by libgxm */
static void *fragmentUsseAlloc( uint32_t size, SceUID *uid, uint32_t *usseOffset )
{
	/* align to memblock alignment for LPDDR */
	size = ALIGN( size, 4096 );

	/* allocate some memory */
	*uid = sceKernelAllocMemBlock( "simple", SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE, size, NULL );
	SCE_DBG_ALWAYS_ASSERT( *uid >= SCE_OK );

	/* grab the base address */
	void *mem = NULL;
	int err = sceKernelGetMemBlockBase( *uid, &mem );
	SCE_DBG_ALWAYS_ASSERT( err == SCE_OK );

	/* map as fragment USSE code for the GPU */
	err = sceGxmMapFragmentUsseMemory( mem, size, usseOffset);
	SCE_DBG_ALWAYS_ASSERT(err == SCE_OK);

	// done
	return mem;
}

/* fragment free used by libgxm */
static void fragmentUsseFree( SceUID uid )
{
	/* grab the base address */
	void *mem = NULL;
	int err = sceKernelGetMemBlockBase( uid, &mem );
	SCE_DBG_ALWAYS_ASSERT( err == SCE_OK );

	/* unmap memory */
	err = sceGxmUnmapFragmentUsseMemory( mem );
	SCE_DBG_ALWAYS_ASSERT( err == SCE_OK );

	/* free the memory block */
	err = sceKernelFreeMemBlock( uid );
	SCE_DBG_ALWAYS_ASSERT( err == SCE_OK );
}

