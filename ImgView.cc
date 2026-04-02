//
//	ImgView.cc	-- ImgView class
//
//----------------------------------------------------------------------
//	18-nov-11  bhb	from Getpic3 ImgView.h
//	Modified:
//	05-dec-11  bhb	add setMode() function here for resizebox init
//					modify drawOverlayGraphics() do draw resize box w/ dashed lines
//					modify handle VM_ZOOM to set resizebox edges
//	11-apr-12  bhb	add _threshImage, DT_THRESH
//	01-aug-12  bhb	remove getRoi(), initRoi(), incorporate into resetsRoi()
//	29-apr-14  bhb	add getPixel()
//	15-oct-14  bhb	change 'thresh'Image etc to 'label' & DT_THRESH to DT_LABLE so generic.
//	16-apr-15  bhb	add _regMesh
//	24-sep-15  bhb	change DisplayImg() to allow DT_MAIN or DT_SPEED with DT_LABEL/FIB
//	12-jul-17  bhb	remove 2D operation
//	07-sep-17  bhb	change _speedImage2D to _initSnakeImg
//	13-sep-17  bhb	rename _initSnakeImg to _initShapeImg
//
#include "Aseg.h"
#include "ImgView.h"
#include "SetBubbles.h"
#include <ColorsUnpacked.h>
#include <ITK/GreyImageWrapper3D.h>
#include <ITK/SpeedImageWrapper3D.h>
#include <ITK/SpeedImageWrapper.h>
#include <ITK/LevelSetImageWrapper3D.h>
#include <ITK/LevelSetImageWrapper.h>
#include <ITK/LabelImageWrapper3D.h>
#include <ITK/LabelImageWrapper.h>
#include <VTK/vtkPolyDataCutter.h>
#include "Mesh/MeshObject.h"
#include <Plot/glFunc.h>			// glError()
#include <Plot/drawFunc.h>
#include <Geom/GeomFcn.h>
#include <vtkSmartPointer.h>
#include <vtkPlane.h>
#include <vtkCutter.h>
#include <vtkStripper.h>
#include <vtkCellArray.h>
#include <Fltk/Fltk.h>				// printEvent()
#include <fltk/events.h>
#include <fltk/visual.h>			// GlWindow mode #defines
#include <fltk/Font.h>
#include <fltk/draw.h>
#include <fltk/ask.h>				// beep() et.al.
#include <X11/cursorfont.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sstream>

using namespace std;
using namespace fltk;

//#define DEBUG_EVENTS
#define DBG1(x)
#define DBG2(x)

// min-max macros
#define MIN(A,B) ((A) < (B) ? (A) : (B))
#define MAX(A,B) ((A) > (B) ? (A) : (B))

//#define GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX 0x9048
//#define GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX 0x9049

/////////////////////////////////////////////////////////////////////////
//
//	class	ImgView : public GLWidget
//
/////////////////////////////////////////////////////////////////////////

ImgView::ImgView( int x, int y, int w, int h)
	: GlWindow( x, y, w, h), _image(0), _speedImage(0), _speedImage2D(0), _initShapeImg(0), 
	_snakeImage(0), _labelImage(0), _mesh(0), _regMesh(0), _segMesh(0), _mode(VM_CROSS_HAIR), 
	_displayType(DT_MAIN), _resizeBoxIndex(-1), _resizeBoxTol(7.F), _editPt(0), _tmpPt(0), 
	_init(true)
{
	_xyTip = new XyTip( BLACK, 215);		// (Color)215 gives error, is lt yellow bg
	resizeBox()->deactivate();
	_sliceSize[0] = _sliceSize[1] = 1.F;

	imgTex().SetGlComponents( 4);					// RGBA
	imgTex().SetGlFormat( GL_RGBA);					// RGBA
	imgTex().SetGlType( GL_UNSIGNED_BYTE);

	snakeTex().SetGlComponents( 4);
	snakeTex().SetGlFormat( GL_RGBA);
	snakeTex().SetGlType( GL_UNSIGNED_BYTE);

	labelTex().SetGlComponents( 4);
	labelTex().SetGlFormat( GL_RGBA);
	labelTex().SetGlType( GL_UNSIGNED_BYTE);
}

ImgView::~ImgView()
{
	xyTip()->hideXY();
	delete xyTip();
}

//
//	Sets _sliceSize[] which determines ROI
//	
void ImgView::setImage( GreyImageWrapper3D *img, vector<float> spacing)
{
	_image = img;
	if ( !img)
		return;
		
	Vec3<uint> size = image()->GetSize();

	_ImageToDisplayTransform = image()->GetImageToDisplayTransform( _id);
	_DisplayToImageTransform = image()->GetDisplayToImageTransform( _id);
	_DisplayToAnatomyTransform = 
		parent()->getGeometry().GetAnatomyToDisplayTransform( _id).Inverse();

	for ( uint d=0; d<3; d++)
	{
		_imageAxes[d] = _DisplayToImageTransform.GetCoordinateIndexZeroBased( d);
		_sliceSize[d] = size[_imageAxes[d]];
		_sliceSpacing[d] = spacing[_imageAxes[d]];
	}

	resetRoi();		// initialize resizeBox
	invalidate();
}

void ImgView::setSpeedImage( SpeedImageWrapper3D *img)	//, const vector<float> &spacing)
{
	_speedImage = img;
	if ( img == 0)
		return;
/*	Vec3<uint> size = speedImage()->GetSize();

	_ImageToDisplayTransform = speedImage()->GetImageToDisplayTransform( _id);
	_DisplayToImageTransform = speedImage()->GetDisplayToImageTransform( _id);
	_DisplayToAnatomyTransform = 
		parent()->getGeometry().GetAnatomyToDisplayTransform( _id).Inverse();

	for ( uint d=0; d<3; d++)
	{
		_imageAxes[d] = _DisplayToImageTransform.GetCoordinateIndexZeroBased( d);
		_sliceSize[d] = size[_imageAxes[d]];
		_sliceSpacing[d] = spacing[_imageAxes[d]];
	}

	resetRoi();		// initialize resizeBox
*/
}

void ImgView::setMode( ViewMode m)
{
	if ( !_image)
		return;

	_mode = m;
	switch ( m)
	{
		case VM_ZOOM:
			resizeBox()->activate();
			break;
		default:
			resizeBox()->deactivate();
			break;
	}
}

void ImgView::initGraphic()
{
	::setCursor( this, XC_crosshair);

//	cout << "GL version: " << glGetString( GL_VERSION) << endl;
}

void ImgView::draw()
{
DBG2( cout << "ImgView::draw()" << endl;)

	if ( !valid())
	{
		Vec2<float> worldSize;

		if ( _init)
		{
			initGraphic();
			_init = false;
		}

		// from itksnap::GenericSliceWindow::OnDraw()
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluOrtho2D( 0.0, w(), 0.0, h());
		glViewport( 0, 0, w(), h());

		// ComputeOptimalZoom()
		if ( image())
			worldSize.set( _sliceSize[0] * _sliceSpacing[0], 
							_sliceSize[1] * _sliceSpacing[1]);
		else
			worldSize.set( 1.F, 1.F);
		m_ViewPosition.set( worldSize[0] * 0.5F, worldSize[1] * 0.5F);
		float r1 = w() / worldSize[0];
		float r2 = h() / worldSize[1];
		m_ViewZoom = r1 < r2 ? r1 : r2;
	}

	glsetfont( HELVETICA, 12.F);	// 12 point

	// Compute the position of the cross-hairs in display space
	Vec3<uint> cur = parent()->cursor();
	Vector3ui cursorImageSpace( cur[0], cur[1], cur[2]);
	Vector3f cursorDisplaySpace = _ImageToDisplayTransform.TransformPoint(
									to_float(cursorImageSpace) + Vector3f(0.5f));

	// Get the current slice number
	_DisplayAxisPosition = cursorDisplaySpace[2];

	// The view is set up to correspond to mm coordinates of the slice
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslated( w() * 0.5, h() * 0.5, 0.0);		// Window (pixel) coords
	glScalef( m_ViewZoom, m_ViewZoom, 1.0);			// zoom image to fill window center in mm
	glTranslatef( -m_ViewPosition[0], -m_ViewPosition[1], 0.0);
	glScalef( _sliceSpacing[0], _sliceSpacing[1], 1.0);			// pixel to mm coords
//	glError( " ImgView::draw: gl err1: ");

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//	if ( damage())
	{
		DisplayImg();
	}

	if ( !can_do_overlay())
		drawOverlayGraphics();

	glError( " ImgView::draw: gl err2: ");
#if 0
	{
		GLint total_mem_kb = 0;
		glGetIntegerv(GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &total_mem_kb);

		GLint cur_avail_mem_kb = 0;
		glGetIntegerv(GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX, &cur_avail_mem_kb);

		cerr << "total: " << total_mem_kb << ", available: " << cur_avail_mem_kb << endl;
	}
#endif
}

void ImgView::draw_overlay()
{
DBG2( cout << "ImgView::draw_overlay()" << endl;)
	float	_x1, _x2, _y1, _y2;		// ortho coords set by setMatrix()
	if ( !valid())
	{
		glViewport(0, 0, (GLint)w()-1, (GLint)h()-1);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluOrtho2D(_x1,	_x2,	_y1,	_y2);
		glMatrixMode(GL_MODELVIEW);
	}
	glClear( GL_COLOR_BUFFER_BIT);

	// set GL ModelView matrix for base image coords
	drawOverlayGraphics();
}

int ImgView::handle( int event)
{
	Pt3<float> pt;
	Pt2<float> pt2;
	
	if ( !_image)
		return 0;

#ifdef DEBUG_EVENTS
	if ( event != DRAG)
		printEvent( "IV: ", event);
#endif
	switch ( _mode)
	{
		case VM_ZOOM:								// do RubberBox
			switch ( event)
			{
				case PUSH:
					switch(event_key())
					{
						case LeftButton:
							// init() also sets box active
							pt = MapWindowToSlice( event_x(), event_y());
							rbIndex() = resizeBox()->getClosestEdge( pt[0], pt[1], 
											rbTol());
							break;
						default:
							break;
					}
					break;
				case DRAG:
					if ( (event_key() == LeftButton) && event_inside( *this))
					{
						if ( resizeBox()->active() && rbIndex() >=0)
							updateRoi( event_x(), event_y());
					}
					break;
				case RELEASE:
					if ( event_key() == LeftButton)
						rbIndex() = -1;
					break;
				default:
					break;
			}
			break;
		case VM_DRAW_WALL:
		case VM_EDIT_WALL:
			if ( parent()->imageAxis(_id) != 2)
				break;
			switch(event)
			{
				case PUSH:
					switch( event_key())
					{
						case LeftButton:
							_tmpPt = new Pt3<float>(mouseLoc( event_x(), event_y()));
							break;
						case MiddleButton:			// move point button.
						case RightButton:			// delete point
						{
							pt = mouseLoc( event_x(), event_y());
							_editPt = parent()->segImage3D()->closestPoint( pt);
							if ( _editPt != 0)
								beep();
						}
							break;
						default:
							break;
					}
					redraw();
					break;
				case DRAG:
					switch(event_key() & 7)	// Mask all but low 3 bits
					{
						case LeftButton:
							//if (me->state & Button1Mask)
							if ( _tmpPt != 0)
								*_tmpPt = mouseLoc( event_x(), event_y());
							break;
						case MiddleButton:
						case RightButton:			// delete point
							if (_editPt)
								*_editPt = mouseLoc( event_x(), event_y());
							break;
						default:
							break;
					}
					redraw();
					break;
				case RELEASE:
					switch(event_key())
					{
						case LeftButton:
							if ( _tmpPt != 0)
							{
								if ( _mode == VM_DRAW_WALL)
									parent()->segImage3D()->addWallPt( *_tmpPt);
								else
									parent()->segImage3D()->insertWallPt( *_tmpPt);
								delete _tmpPt;
								_tmpPt = 0;
							}
							break;
						case MiddleButton:
							_editPt = 0;			// reset edit
							parent()->segImage3D()->setCurWallChanged();
							break;
						case RightButton:			// delete point
							if ( _editPt)
							{
								parent()->segImage3D()->deleteWallPt( *_editPt);
								_editPt = 0;
							}
						default:
							break;
					}
					redraw();
					break;
				case MOVE:					// no button pressed, set current wall if any
					// Have to click mouse button in ImgView window to select before 
					// mouseLoc() will work correctly.
					if ( event_state() & SHIFT)
						break;		// allow moving past a selected interior wall so it
									// stays selected for 'delete'.
					if ( _mode == VM_EDIT_WALL)
					{
						pt = mouseLoc( event_x(), event_y());
//						cout << "pt: " << pt << endl;
						if ( parent()->segImage3D()->closestEdge( pt) > -1)	// sets m_CurWall
						{
							parent()->activateEditMenu();
							redraw();
						}
					}
				default:
					break;
			}
			break;
			
		case VM_DRAW_CIRCLE:
			switch(event)
			{
				case PUSH:
					make_current();	// set our Graphics Context.
					switch( event_key())
					{
						case LeftButton:	// use _centerPt for center
							pt = mouseLoc( event_x(), event_y());
							_centerPt.set( pt.x(), pt.y());
							cout << "ImgView: _centerPt " << _centerPt << endl;
							break;
						default:
							break;
					}
					break;
				case DRAG:
					switch(event_key() & 7)	// Mask all but low 3 bits
					{
						case LeftButton:
							//if (me->state & Button1Mask)
							pt = mouseLoc( event_x(), event_y());
							pt2.set( pt.x(), pt.y());
							_radius = dist( _centerPt, pt2);
							Circle( _centerPt, _radius, 30, _poly);
							_poly.reverse();			// make circle clockwise
							redraw();
							break;
					}
					break;
				case RELEASE:
					switch(event_key())
					{
						case LeftButton:
							if ( _tmpPt != 0)
							{
								delete _tmpPt;
								_tmpPt = 0;
							}
							redraw();
							break;
						default:
							break;
					}
					break;
				default:
					break;
			}
			break;
		case VM_DISTANCE:
			switch(event)
			{
				case PUSH:
					switch(event_key())
					{
						case LeftButton:
							if ( _tmpPt != 0)
								delete _tmpPt;
							_tmpPt = new Pt3<float>(mouseLoc( event_x(), event_y()));
							redraw();
							::setCursor( this, XC_plus);
							break;
						default:
							break;
					}
					break;
				case DRAG:
					// display distance from _tmpPt to current location in mm
					if ( _tmpPt && event_inside( *this))
					{
						Pt3<float> pt3f = mouseLoc( event_x(), event_y());
						Pt3<float> tmpPt( (*_tmpPt)[0] * _sliceSize[0], 
											(*_tmpPt)[1] * _sliceSize[1]);
						pt3f[0] *= _sliceSize[0];	// convert to mm
						pt3f[1] *= _sliceSize[1];	// convert to mm
						float d = dist( pt3f, *_tmpPt);
						xyTip()->showX( d);
					}
					break;
				default:
					break;
			}
			break;
		case VM_DIST_OFF:
			if ( _tmpPt != 0)
			{
				delete _tmpPt;
				_tmpPt = 0;
			}
			showXYoff();
			_mode = VM_CROSS_HAIR;
			::setCursor( this, XC_crosshair);
			redraw();
			break;
		case VM_POINT:									// display cursor image x, y
			if ( event == PUSH || event == DRAG)
			{
				pt = mouseLoc( event_x(), event_y());
//i				if ( event_key() == RightButton)
//i					pt = Img()->imgSpace( pt);
				xyTip()->showXY( int(pt[0]), int(pt[1]));
			}
			else if ( event == RELEASE)
				xyTip()->hideXY();
			break;
		case VM_CROSS_HAIR:
		case VM_SEEDS:
		case VM_BUBBLE:
			if ( event == PUSH || event == DRAG)
			{
				pt = MapWindowToSlice( event_x(), event_y());
				xyTip()->showXY( int(pt[0]), int(pt[1]));
				pt = MapSliceToImage( pt);
				parent()->setCrosshairs( pt);
			}
			else if ( event == RELEASE)
				xyTip()->hideXY();
			break;
		case VM_PIXVAL:									// display cursor image pixel value
			switch (event)
			{
				case PUSH:
				{
					pt = MapWindowToSlice( event_x(), event_y());
					pt = MapSliceToImage( pt);
					float pixel = getPixel( pt);				
					xyTip()->showX( pixel);
				}
					break;
				case DRAG:
				{
					pt = MapWindowToSlice( event_x(), event_y());
					pt = MapSliceToImage( pt);
					float pixel = getPixel( pt);				
					xyTip()->showX( pixel);
				}
					break;
				case RELEASE:
					xyTip()->hideXY();
					break;
				default:
					break;
			}
			break;
		case VM_DETAIL:
		case VM_NONE:
			break;
		default:
			break;
	}
	return 1;
}

//
//	reset to default mode
//
void
ImgView::reset()
{
//i	Img()->Walls()->resetCurrent();
	setMode( VM_CROSS_HAIR);
	_editPt = _tmpPt = 0;

	redraw();
}

//
//	Initialize resizeBox from parent ROI.
//	Called from Aseg::setRoi() & resetROI().
//
void
ImgView::resetRoi()
{
	Pt3<float> roi[2];
	itk::Index<3u> ul = parent()->segROI().GetIndex();
	itk::Size<3u> sz = parent()->segROI().GetSize();
	Pt3<float> v0( (float)ul[0], (float)ul[1], (float)ul[2]);
	Pt3<float> v1( (float)sz[0], (float)sz[1], (float)sz[2]);
	v1 += v0;
	roi[0] = MapImageToSlice( v0);				// to ModelView coords
	roi[1] = MapImageToSlice( v1);

	Pt2<float> p0( roi[0][0], roi[0][1]);
	Pt2<float> p1( roi[1][0], roi[1][1]);
//	cout << "resetRoi: (" << _id << ") [" << p0 << "], [" << p1 << "]" << endl;
	resizeBox()->init( p0, p1);
}

//
//	Display the image in OpenGL drawing area.	Called from draw().
//	
//	GetDisplaySlice() for LabelImageWrapper3D images (_labelImage uses
//	LabelToRGBAFilter to convert ColorLabelTable colors to RGBA).
//	For other image types, other filters are used to generate RGBA display slice.
//	
void ImgView::DisplayImg()
{
	string BadImage = "Bad image, can't display";
	if ( !_image)
		return;

	Vec3<double> clrBackground( 1., 1., 1.);

	if ( getDisplayType() & DT_MAIN)
	{
		if ( !imgTex().SetImage( image()->GetDisplaySlice( _id).GetPointer()))
			alert( BadImage.c_str());
		else
			imgTex().Draw( clrBackground);
	}
	if ( getDisplayType() & DT_SPEED)				// filtered image to segment
	{
		if ( speedImage2D())
		{
			if ( _id == 0)		// 2D Edge or Threshold Filter, Axial only
			{
				SpeedImageWrapper::DisplaySliceType *img = 
					speedImage2D()->GetDisplaySlice().GetPointer();
				img->Update();
				if ( !imgTex().SetImage( img))
					alert( BadImage.c_str());
				else
					imgTex().DrawTransparent( 128);
			}
		}
		else if ( speedImage())
		{
			if ( !imgTex().SetImage( speedImage()->GetDisplaySlice( _id).GetPointer()))
				alert( BadImage.c_str());
			else
				imgTex().Draw( clrBackground);
		}
		else
			cerr << "ImgView::DisplayImg: DT_SPEED but no speedImage" << endl;
	}
	if ( getDisplayType() & DT_SNAKE)				// snake image
	{
		if ( !snakeImage())
		{
			cerr << "ImgView::DisplayImg: DT_SNAKE but no snakeImage" << endl;
			return;
		}
		if ( !snakeTex().SetImage( snakeImage()->GetDisplaySlice( _id).GetPointer()))
			alert( BadImage.c_str());
		else
			snakeTex().DrawTransparent( 128);
	}
	if ( (getDisplayType() & DT_LABLE) || (getDisplayType() & DT_FIB))	// segmentation image
	{
		if ( !labelImage())
		{
			cerr << "ImgView::DisplayImg: DT_LABEL but no labelImage" << endl;
			return;
		}

		if ( !labelTex().SetImage( labelImage()->GetDisplaySlice( _id).GetPointer()))
			alert( BadImage.c_str());
		else
			labelTex().DrawTransparent( 128);	// was 192
	}
	if ( getDisplayType() & DT_INIT)			// init shape image
	{
		if ( initShapeImg())
		{
			LabelImageWrapper::DisplaySliceType *img = 
				initShapeImg()->GetDisplaySlice( _id).GetPointer();
//			img->Update();
			if ( !labelTex().SetImage( img))
				alert( BadImage.c_str());
			else
				labelTex().DrawTransparent( 128);
		}
	}
	glError( "ImgView::DisplayImg: gl err: ");
}

//**********************************************************************
//	draw in overlay planes
//**********************************************************************
void
ImgView::drawOverlayGraphics()
{
	if ( resizeBox()->active())
	{
		can_do_overlay() ? glIndexi(1) : glColor4ub( RED_UP);
		glPushAttrib(GL_LINE_BIT | GL_COLOR_BUFFER_BIT);
		glEnable(GL_LINE_STIPPLE);
		glLineStipple( 3, 0x9999); 	// 1001 1001 1001 1001
		resizeBox()->draw();
		glPopAttrib();
	}

	if ( _tmpPt)
	{
		can_do_overlay() ? glIndexi(3) : glColor4ub( YELLOW_UP);
		drawCircle( *_tmpPt, 2.0);
	}
	if ( _editPt)
	{
		can_do_overlay() ? glIndexi(1) : glColor4ub( RED_UP);
		drawCircle( *_editPt, 2.0);
	}

	// draw poly (circle)
	if ( _poly.size())
	{
		can_do_overlay() ? glIndexi(3) : glColor4ub( YELLOW_UP);
		_poly.draw2d();
	}

	// display bubbles, mesh or walls
	if ( image())
	{
		drawCrosshairs();
		glPushMatrix();
		glLoadIdentity();
			drawScale();
			drawAxisLabels();
			drawSliceNumber();
		glPopMatrix();

		// draw bubbles if present, else either mesh or regMesh
		if ( (getMode() == VM_BUBBLE) || (getDisplayType() & DT_BUBBLES))
			drawBubbles();

		// draw either mesh or regMesh
		if ( mesh() && mesh()->GetNumberOfVTKMeshes() && !parent()->registrationOn() &&
					parent()->showMeshToggle())
		{
			drawMesh( mesh());
		}
		else if ( regMesh() && regMesh()->GetNumberOfVTKMeshes() && 
					( parent()->registrationOn() || parent()->showRegMeshToggle()))
		{
			drawMesh( regMesh());
		}

		// also draw segMesh if present and requested or else walls in Axial view if present
		if ( segMesh() && segMesh()->GetNumberOfVTKMeshes() && 
					parent()->showSegmentationToggle())
		{
			drawMesh( segMesh());
		}
		else if ( parent()->imageAxis(_id) == 2)
			parent()->segImage3D()->drawWalls();
	}
	glError( "ImgView::drawOverlayGraphics: gl err: ");
}

void
ImgView::drawMesh( MeshObject *mesh)
{
	uint idx[3][2] = { {0, 1}, {1, 2}, {0, 2}};
	uint x = idx[ getId()][0];
	uint y = idx[ getId()][1];
	Vec3<uint> sliceIndx = image()->GetSliceIndex();
	itk::Size<3u> sz = parent()->mainROI().GetSize();
	
	mesh->GetVTKMesh( 0)->ComputeBounds();
	double *bnds = mesh->GetVTKMesh( 0)->GetBounds();

	typedef vnl_matrix_fixed<double, 4, 4> Mat4d;
	Mat4d niftiXform = image()->GetNiftiSform();
	Mat4d niftiInvXform = image()->GetNiftiInvSform();

	// slice m_Meshes at plane of this slice
	Vector3d v3d( sliceIndx[0], sliceIndx[1], sliceIndx[2]);
	Vector3d vtran = affine_transform_point( niftiXform, v3d);
	Vec4<float> pln = getSlicePlane();
	bool	neg = niftiXform[1][1] < 0.;		// big kludge
	
	glLineWidth( 2.0F);

	for ( int i=0; i<mesh->GetNumberOfVTKMeshes(); i++)
	{
		vtkSmartPointer<vtkPlane> plane = vtkSmartPointer<vtkPlane>::New();
		plane->SetOrigin( double( vtran[0]), double( vtran[1]), double( vtran[2]));
		plane->SetNormal( double( pln[0]), double( pln[1]), double( pln[2]));

		vtkSmartPointer<vtkCutter> cutter = vtkSmartPointer<vtkCutter>::New();
		cutter->SetInputData( mesh->GetVTKMesh( i));
		cutter->SetCutFunction(plane);
		cutter->Update();

		vtkPolyData *pd = cutter->GetOutput();
		vtkPoints *points = pd->GetPoints();
		vtkCellArray *cells = pd->GetLines();

		const ColorLabel &cl = parent()->GetColorLabelTable()->GetColorLabel( 
								mesh->GetVTKMeshLabel( i));
		mesh->ApplyColorLabel( cl);		// calls glColor
		vtkIdType *indices;
		vtkIdType numberOfPoints;
		unsigned int lineCount = 0;
		for ( cells->InitTraversal(); cells->GetNextCell(numberOfPoints, indices); 
				lineCount++)
		{
			glBegin( GL_LINE_LOOP);
			for (vtkIdType i = 0; i < numberOfPoints; i++)
			{
				Vector3d v3d;
				points->GetPoint( indices[i], v3d.data_block());
				Vector3d vtranInv = affine_transform_point( niftiInvXform, v3d);
				if ( neg)	// kludge
				{
					if ( getId() == 0)	// Axial needs y invert
						vtranInv[y] = sz[y] - vtranInv[y];
				}
				else
				{
					if ( getId() != 0)	// Sagital & Coronal need y invert
						vtranInv[y] = sz[y] - vtranInv[y];
					if ( getId() == 1)	// Sagital right-left reversed
						vtranInv[x] = sz[x] - vtranInv[x];
				}
				glVertex2d( vtranInv[x], vtranInv[y]); 
			}
			glEnd();
		}
	}
	glLineWidth( 1.0F);
}
//
//	from UserInterface/SliceWindow/GenericSliceWindow::DrawRulers()
//
void
ImgView::drawScale()
{
	int wd = w(), ht = h();

	glColor4ub( GREEN_UP);

	// scale not more than half display width
	double maxw = 0.5 * wd - 20.0;
	maxw = maxw < 5 ? 5 : maxw;

	double scale = 1.0;
	while( m_ViewZoom * scale > maxw) scale /= 10.0;
	while( m_ViewZoom * scale < 0.1 * maxw) scale *= 10.0;

	// Draw a zoom bar
	double bw = scale * m_ViewZoom;
	glBegin(GL_LINES);
	glVertex2d( 5, ht - 5);
	glVertex2d( 5, ht - 15);
	glVertex2d( 5, ht - 10);
	glVertex2d( 5 + bw, ht - 10);
	glVertex2d( 5 + bw, ht - 5);
	glVertex2d( 5 + bw, ht - 15);
	glEnd();

	// Based on the log of the scale, determine the unit
	string unit = "mm";
	if (scale >= 10 && scale < 1000)
	{ unit = "cm"; scale /= 10; }
	else if (scale >= 1000)
	{ unit = "m"; scale /= 1000; }
	else if (scale < 1 && scale > 0.001)
	{ unit = "\xb5m"; scale *= 1000; }
	else if (scale < 0.001)
	{ unit = "nm"; scale *= 1000000; }

	ostringstream oss;
	oss << scale << " " << unit;

	// See if we can squeeze the label under the ruler
	if (bw > 30)
		glRasterPos2f( 10, ht - 30);
	else
		glRasterPos2f( bw+10, ht - 15);
	gldrawtext( oss.str().c_str());
}

//
//	from UserInterface/SliceWindow/CrosshairsInteractionMode::OnDraw()
//
void
ImgView::drawCrosshairs()
{
	// Get the current cursor position
	Vec3<uint> xCursorInteger = parent()->cursor();
	Pt3<float> xCursorImage( xCursorInteger[0], xCursorInteger[1], xCursorInteger[2]);

	// Shift the cursor position by by 0.5 in order to have it appear between voxels
	Vec3<float> inc( 0.5F);
	xCursorImage += inc;

	// Get the cursor position on the slice
	Pt3<float> xCursorSlice = MapImageToSlice( xCursorImage);

	glColor4ub( BLUE_UP);

	// Refit matrix so that the lines are centered on the current pixel
	glPushMatrix();
	glTranslatef( xCursorSlice[0], xCursorSlice[1], 0.0);

	// Paint the cross-hairs
	glBegin(GL_LINES);
	glVertex2f( -xCursorSlice[0], 0); glVertex2f( _sliceSize[0] - xCursorSlice[0], 0);
	glVertex2f( 0, -xCursorSlice[1]); glVertex2f( 0, _sliceSize[1] - xCursorSlice[1]);
	glEnd();

	glPopMatrix();
}

void
ImgView::drawAxisLabels()
{
	// The letter labels
	static const char *letters[3][2] = {{"R","L"},{"A","P"},{"I","S"}};
	static const char *axisNames[3] = { "Axial", "Sagital", "Coronal"};
	const char *labels[2][2];

	// Repeat for X and Y directions
	for(unsigned int i=0;i<2;i++)
	{
		// Which axis are we on in anatomy space?
		unsigned int axis = _DisplayToAnatomyTransform.GetCoordinateIndexZeroBased(i);

		// Which direction is the axis facing (returns -1 or 1)
		int axisDirection = _DisplayToAnatomyTransform.GetCoordinateOrientation(i);

		// Map the direction onto 0 or 1
		unsigned int letterIndex = (1 + axisDirection) >> 1;

		// Compute the two labels for this axis
		labels[i][0] = letters[axis][1-letterIndex];
		labels[i][1] = letters[axis][letterIndex];
	}

	glColor4ub( ORANGE_YELLOW_UP);	// GOLD_UP, GOLD2_UP, PEACH_UP

	int offset = 12;
	int margin = 4;
	int wd = w(), ht = h();

	glRasterPos2i( margin, ht/2);
	gldrawtext( labels[0][0]);
	glRasterPos2i( wd - offset, ht/2);
	gldrawtext( labels[0][1]);
	glRasterPos2i( wd/2, margin);
	gldrawtext( labels[1][0]);
	glRasterPos2i( wd/2, ht - offset);
	gldrawtext( labels[1][1]);

	// draw axis name in lower left corner
	glColor4ub( GREEN_UP);
	glRasterPos2i( margin, margin);
	gldrawtext( axisNames[ _id]);
}

void
ImgView::drawSliceNumber()
{
	ostringstream oss;
	int slice = parent()->getSlice( _id);

	oss << slice;
	glColor4ub( GREEN_UP);

	glRasterPos2i( w() - 25, 5);
	gldrawtext( oss.str().c_str());
}

//
//	From UserInterface/SliceWindow/BubblesInteractionMode.cxx
//	
void
ImgView::drawBubbles()
{
	vector<Sphere<uint> > &bubbles = parent()->bubbles();
	int numBubbles = bubbles.size();

	if ( numBubbles == 0)
		return;
	int activeBubble = parent()->bubblesUI()->activeBubble();
	unsigned char alpha = 128;		// m_GlobalState->GetSegmentationAlpha();
	uchar rgb[3];
	uchar rgbActive[3];
	int currentcolor = parent()->GetDrawingColorLabel();
	ColorLabel cl = parent()->GetColorLabelTable()->GetColorLabel(currentcolor);

//	if ( !cl.IsValid())
//		return;

	cl.GetRGBVector(rgb);
	for ( int i=0; i<3; i++)
		rgbActive[i] = 255 - (255 - rgb[i]) / 2;

	// Get the current crosshairs position
	Vec3<uint> cur = parent()->cursor();
	Vec3<float> cursorImage( float(cur.x()) + 0.5F, 
							float(cur.y()) + 0.5F, 
							float(cur.z()) + 0.5F);		

	// Get the image space dimension that corresponds to this window
	int iid = _imageAxes[2];

	// Get the other essentials from the parent
	Vec3<float> scaling = _sliceSpacing;

	// Turn on alpha blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Create a filled circle object
	GLUquadricObj *object = gluNewQuadric();
	gluQuadricDrawStyle(object,GLU_FILL);

	// Draw each bubble
	for (int i = 0; i < numBubbles; i++)
	{

		// Get the center and radius of the i-th bubble
		Pt3<uint> ctr = bubbles[i].center;
		Pt3<float> ctrImage( to_float(ctr));
		double radius = bubbles[i].radius;

		// Remap the center into slice coordinates
		Pt3<float> ctrSlice = MapImageToSlice( ctrImage);

		// Compute the offset from the center along the slice z-direction
		// in physical coordinates
		double dcenter = scaling[2] * (cursorImage[iid] - ctrImage[iid]);

		// Check if the bubble is intersected by the current slice plane
		if (dcenter >= radius || -dcenter >= radius) continue;

		// Compute the radius of the bubble in the cut plane
		double diskradius = sqrt(fabs(radius*radius - dcenter*dcenter));

		// Draw the bubble
		if ( i == activeBubble)
			glColor4ub( rgbActive[0], rgbActive[1], rgbActive[2], alpha);
		else
			glColor4ub( rgb[0], rgb[1], rgb[2], alpha);
		glPushMatrix();

		glPushAttrib(GL_POLYGON_BIT | GL_COLOR_BUFFER_BIT);

		glTranslatef( ctrSlice[0], ctrSlice[1], 0.0f);
		glScalef( 1.0f / scaling[0], 1.0f / scaling[1], 1.0f);
		gluDisk( object, 0, diskradius, 100, 1);
		glPopAttrib();
		glPopMatrix();
	}

	gluDeleteQuadric(object);
	glDisable(GL_BLEND);
}

//
//	Map a point in slice coordinates to image coordinates
//
Pt3<float>
ImgView::MapSliceToImage( const Pt3<float> &pt)
{
	// Get corresponding position in image space
	Vector3f xSlice( pt[0], pt[1], pt[2]);
	xSlice = _DisplayToImageTransform.TransformPoint(xSlice);
	Pt3<float> ipt( xSlice(0), xSlice(1), xSlice(2));
	return ipt;
}

//
//	Map a point in image coordinates to slice coordinates
//
Pt3<float>
ImgView::MapImageToSlice( const Pt3<float> &imgPt)
{
		// Get corresponding position in display space
		Vector3f xImage( imgPt[0], imgPt[1], imgPt[2]);
		Vector3f xSlice = _ImageToDisplayTransform.TransformPoint(xImage);
		return Pt3<float>( xSlice(0), xSlice(1), xSlice(2));
}

//
//	Convert slice coords to ModelView coords
//	
Pt3<float>
ImgView::MapSliceToWindow( const Pt3<float> &xSlice)
{
	// Adjust the slice coordinates by the scaling amounts
	Vec2<float> uvScaled( xSlice[0] * _sliceSpacing[0], xSlice[1] * _sliceSpacing[1]);

	// Compute the window coordinates
	Vec2<float> uvWindow(((uvScaled - m_ViewPosition) * m_ViewZoom) +
							Vec2<float>(0.5F * w(), 0.5F * h()));

	// That's it, the projection matrix is set up in the scaled-slice coordinates
	Pt3<float> pt( uvWindow[0], uvWindow[1], 0.F);
	return pt;
}

//
//	Convert Model View coords to slice coords
//	
Vec3<float>
ImgView::MapWindowToSlice( const int x, const int y)
{
	// Compute the scaled slice coordinates
	Vec2<float> xyWindow( x, float(h()) - y);
	Vec2<float> winCenter( w() * 0.5F, h() * 0.5F);
	Vec2<float> uvScaled = m_ViewPosition + (xyWindow - winCenter) / m_ViewZoom;

	// The window coordinates are already in the scaled-slice units
	Vec3<float> uvSlice( uvScaled[0] / _sliceSpacing[0], uvScaled[1] / _sliceSpacing[1],
							_DisplayAxisPosition);

	// Return this vector
	return uvSlice;
}

//
//	convert point x,y from X-window coords to Model View coords and return it.
//
Pt3<float>
ImgView::mouseLoc( int wx, int wy)
{
	// Convert the event coordinates into the model view coordinates
	// Returns point in image coords not mm
	double modelMatrix[16], projMatrix[16];
	GLint viewport[4];
	glGetDoublev(GL_MODELVIEW_MATRIX, modelMatrix);
	glGetDoublev(GL_PROJECTION_MATRIX, projMatrix);
	glGetIntegerv(GL_VIEWPORT,viewport);

	// Projection works with doubles, event is a float
	Vec3<double> xProjection;
	gluUnProject( wx, h() - 1 - wy, 0,
				 modelMatrix, projMatrix, viewport,
				 &xProjection[0],&xProjection[1],&xProjection[2]);
	return Pt3<float>( xProjection[0], xProjection[1], xProjection[2]);
}

//
//	Called from handle() -> VM_ZOOM
//	
void
ImgView::updateRoi( int wx, int wy)
{
	if ( rbIndex() < 0)
		return;

	// get full image region for clamping
	Pt3<float> clampRegion[2];
	Aseg::RegionType roiFull = parent()->mainROI();
	itk::Index<3u> ul = roiFull.GetIndex();
	itk::Size<3u> sz = roiFull.GetSize();
	clampRegion[0].set( (float)ul[0], (float)ul[1], (float)ul[2]);
	clampRegion[1].set( (float)sz[0], (float)sz[1], (float)sz[2]);
	clampRegion[1] += clampRegion[0];

	Vec3<float> pt = MapWindowToSlice( wx, wy);		// get point in slice coords
	pt = MapSliceToImage( pt);						// now in image coords
	// clamp to image region
	for ( uint i=0; i<3; i++)
	{
		if ( pt[i] < clampRegion[0][i])
			pt[i] = clampRegion[0][i];
		if ( pt[i] > clampRegion[1][i])
			pt[i] = clampRegion[1][i];
	}

	pt = MapImageToSlice( pt);						// back to slice
	resizeBox()->setEdge( rbIndex(), pt[0], pt[1]);

	// Update the region of interest in the system
	Pt3<float> region[2];
	region[0].set( resizeBox()->start()[0], resizeBox()->start()[1], _DisplayAxisPosition);
	region[1].set( resizeBox()->end()[0], resizeBox()->end()[1], _DisplayAxisPosition);
	region[0] = MapSliceToImage( region[0]);
	region[1] = MapSliceToImage( region[1]);
	region[1] -= region[0];			// now is size
	for ( int i=0; i<3; i++)
	{
		ul[i] = static_cast<unsigned long>(region[0][i]+0.5F);
		if ( ul[i] < clampRegion[0][i])
			ul[i] = clampRegion[0][i];
		if ( ul[i] > clampRegion[1][i])
			ul[i] = clampRegion[1][i];
		sz[i] = static_cast<unsigned long>(region[1][i]+0.5F);
		if ( sz[i] > (clampRegion[1][i] - ul[i]))
			sz[i] = (clampRegion[1][i] - ul[i]);
	}

//	cout << "updateRoi: (" << _id << ") [" << ul[0] << "," << ul[1] << "," << ul[2] << "], [" 
//		<< sz[0] << "," << sz[1] << "," << sz[2] << "]" << endl;
	Aseg::RegionType roi;
	roi.SetIndex( ul);
	roi.SetSize( sz);

	// maintain z-direction index & size
	Aseg::RegionType roiPrev = parent()->segROI();
	unsigned int idx = _imageAxes[2];
	roi.SetIndex( idx, roiPrev.GetIndex(idx));
	roi.SetSize( idx, roiPrev.GetSize(idx));
	parent()->setRoi( roi);			// sets _segROI, calls ImgView::resetROI()
	parent()->redraw();
}

Vec4<float>
ImgView::getSlicePlane()
{
	Pt3<float> pt1, pt2, pt3;
	itk::Size<3u> sz = parent()->mainROI().GetSize();
	Vec3<float> pt = MapWindowToSlice( 0.F, 0.F);		// get point in slice coords
	pt1 = MapSliceToImage( pt);							// now in image coords
	pt = MapWindowToSlice( 0.F, float(sz[1]));
	pt2 = MapSliceToImage( pt);
	pt = MapWindowToSlice( float(sz[0]), float(sz[1]));
	pt3 = MapSliceToImage( pt);

	return implPlane( pt1, pt2, pt3);
}

//----------------------------------------------------------------------
//	Display menu functions
//----------------------------------------------------------------------

void
ImgView::showXY( Pt3<float> &pt)
{
	xyTip()->showXY( pt[0], pt[1]);
//	::setCursor( this, XC_plus);
}

void
ImgView::setCursor( Vec3<uint> &cursor, Pt3<float> &pt)
{
	itk::Size<3u> sz = parent()->segROI().GetSize();
	
	for ( uint dim = 0; dim < 3; dim++)
	{
		float val = pt[dim];
		if ( val < 0.F)
			val = 0.F;
		if ( val >= sz[dim])
			val = sz[dim] - 1;
		cursor[dim] = uint(val);
	}
}

float
ImgView::getPixel( Pt3<float> &pt)
{
	float pixel = 0.F;
	Vec3<uint> cursor;
	
	setCursor( cursor, pt);
	switch ( getDisplayType())
	{
		case DT_MAIN:
			pixel = image()->GetVoxel( cursor);
			break;
		case DT_SPEED:		// intensity image
			if ( speedImage())
				pixel = speedImage()->GetVoxel( cursor);
			break;
		case DT_SNAKE:		// segmentation image
			if ( snakeImage())
				pixel = snakeImage()->GetVoxel( cursor);
			break;
		case DT_LABLE:
		case DT_FIB:
			if ( labelImage())
				pixel = labelImage()->GetVoxel( cursor);
			break;
	}
	return pixel;
}
