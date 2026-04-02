//
//	MeshView.cc - class for viewing 3D ITK mesh object
//	
//	29-dec-11	bhb
//	Modified:
//	10-feb-12  bhb	fix rotation in drawModel - translate before/after
//	31-jul-13  bhb	add ViewMode enum, _viewMode, setViewMode() for Pic3Model registration
//					add rotateMesh()
//	03-jun-14  bhb	add displayImg()
//	09-jun-14  bhb	fixed zoom & translate mesh
//	31-mar-15  bhb	fixed Registration transform drawing in drawModel()
//	16-apr-15  bhb	add _regMesh
//	23-feb-16  bhb	make _modTransMatrix, _modRotMatrix arrays, add #define MAX_MESH_WALLS
//	24-feb-16  bhb	add _startX[], _startY[] for mesh registration
//	
#include "MeshView.h"
#include "Aseg.h"
#include <ITK/GreyImageWrapper3D.h>
#include "Mesh/MeshObject.h"
#include <ColorsUnpacked.h>
#include <Fltk/Fltk.h>				// printEvent()
#include <fltk/run.h>				// timeouts
#include <fltk/events.h>
#include <fltk/visual.h>			// GlWindow mode #defines
#include <fltk/Font.h>
#include <fltk/draw.h>
#include <fltk/ask.h>
#include <X11/cursorfont.h>

using namespace std;
using namespace fltk;		// still need 'fltk::' for timeout funcs for some reason

#define DMV(x)						// display GL_MODELVIEW matrix

MeshView::MeshView( int x, int y, int w, int h) : ModelView( x, y, w, h), 
	_parent(0), _init(true), _image(0), _mesh(0), _regMesh(0), _segMesh(0), 
	_viewMode( VM_NONE), _modZoom(1.F)
{
	for ( uint wt=0; wt<5; wt++)
		_showMesh[wt] = false;

	for ( uint i=0; i<3; i++)
	{
		imgTex(i).SetGlComponents( 4);					// RGBA
		imgTex(i).SetGlFormat( GL_RGBA);				// RGBA
		imgTex(i).SetGlType( GL_UNSIGNED_BYTE);
	}
}

MeshView::~MeshView()
{
}

void
MeshView::resetRegistration()
{
	for ( uint i=0; i < MAX_MESH_WALLS; i++)
	{
		_modRotMatrix[i].identity();
		_modTransMatrix[i].identity();
	}
	_modZoom = 1.F;
}

void
MeshView::initialize( Aseg *p, MeshObject *mesh)
{
	_parent = p;
	_mesh = mesh;

	if ( !parent()->image())
		return;

	GreyImageWrapper3D *image = parent()->mainImage()->GetGrey();

	Aseg::RegionType roi = parent()->segROI();
	itk::Index<3u> in = roi.GetIndex();
	itk::Size<3u> sz = roi.GetSize();

	for ( uint i=0; i<3; i++)
		_imageAxes[i] = image->GetDisplaySliceImageAxis( i);

	_imageSize.set( sz[_imageAxes[0]], sz[_imageAxes[1]], sz[_imageAxes[2]]);
	vector<float> spc = parent()->mainImage()->PixelSpacing();
	_pixelSpacing.set( (double)spc[0], (double)spc[1], (double)spc[2]);
	
	// world transform
	m_WorldMatrix = image->GetNiftiSform();

	_minPt.set( in[0], in[1], in[2]);
	_maxPt.set( in[0] + sz[0], in[1] + sz[1], in[2] + sz[2]);

	_Bbox.init( _minPt, _maxPt);

	initView( _Bbox);		// initializes _centerPt and other stuff
}

//
//	Overload ModelView::initGraphic()
//	
void 
MeshView::initGraphic()
{
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glEnable( GL_DEPTH_TEST);

	// Set up the materials
	GLfloat light0Pos[4] = { 0.0F, 0.0F, 1.0F, 0.0F};
	GLfloat matAmb[4] = { 0.7F, 0.7F, 0.7F, 1.00F};
	GLfloat matDiff[4] = { 0.65F, 0.65F, 0.65F, 1.00F};
	GLfloat matSpec[4] = { 0.30F, 0.30F, 0.30F, 1.00F};
	GLfloat matShine = 10.0F;

	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, matAmb);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, matDiff);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpec);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, matShine);
	glEnable(GL_COLOR_MATERIAL);

	// Setup Lighting
	glLightfv(GL_LIGHT0, GL_AMBIENT, matAmb);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, matDiff);
	glLightfv(GL_LIGHT0, GL_SPECULAR, matSpec);
	glLightfv(GL_LIGHT0, GL_POSITION, light0Pos);
	glEnable(GL_LIGHT0);
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glClearDepth( 1);

	glsetfont( HELVETICA, 12.F);	// 12 point
	setCursor( this, XC_crosshair);
}

void 
MeshView::draw()
{
	GLdouble	fovy, aspect;

	make_current();
	if ( !valid())
	{
		if ( _init)
		{
			initGraphic();
			_init = false;
		}

		glViewport( 0, 0, w(), h());

		_cenx = float(w()) * 0.5F;
		_ceny = float(h()) * 0.5F;
	}
	glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	glMatrixMode( GL_PROJECTION);
	glLoadIdentity();
	double x, y, z;
	z = (_imageSize[2] * _pixelSpacing[2] * 0.7 + 1.0) * 4.0 / _zoom;
	if ( _imageSize[0] * _pixelSpacing[0] > _imageSize[1] * _pixelSpacing[1])
	{
		double max = _imageSize[0] * _pixelSpacing[0] * 0.7 + 1.0;
		x = max / _zoom;
		y = h() * x / w();
	}
	else
	{
		double max = _imageSize[1] * _pixelSpacing[1] * 0.7 + 1.0;
		y = max / _zoom;
		x = w() * y / h();
	}
	glOrtho( -x, x, -y, y, -z, z);

	drawModel();
}

//
//	Overload ModelView::handle()
//	
int
MeshView::handle( int event)
{
	bool handled = false;
	switch ( _viewMode)
	{
		case VM_NONE:
			break;
		case VM_REGISTER:		// for rotate/translate (Pic3) Mesh relative to cube
			// Button functions:
			//	L - x/y rot, M - z rot, R - zoom
			//	Shift L - translate
			switch (event)
			{
				case PUSH:
					fltk::remove_timeout( rotateMeshTimerCB, (void *)this);
					_mousex = float(event_x());		// window coord (pixel)
					_mousey = float(h() - event_y());
					switch (event_key())
					{
						case LeftButton:
							if ( event_state() & SHIFT)
							{
								for ( uint wt=0; wt < 5; wt++)
								{
									_startX[wt] = _mousex;
									_startY[wt] = _mousey;
								}
								_rotMode = XY_TRANS;
								setCursor( this, XC_fleur);
								changedCursor( true);
							}
							else
							{
								if ( XYRotOK())
									_rotMode = XY_ROT;
							}
							break;
						case MiddleButton:
							_rotMode = Z_ROT;
							setCursor( this, XC_exchange);
							changedCursor( true);
							break;
						case RightButton:
							for ( uint wt=0; wt < 5; wt++)
								_startY[wt] = _mousey;
							_rotMode = ZOOM_ROT;
							setCursor( this, XC_double_arrow);
							changedCursor( true);
							break;
					}
					if ( _rotMode != NO_ROT)
						fltk::add_timeout( ROTATE_INTERVAL, rotateMeshTimerCB, (void *)this);

					break;
				case RELEASE:
					if ( _rotMode != NO_ROT)
					{
						fltk::remove_timeout( rotateMeshTimerCB, (void *)this);
						_rotMode = NO_ROT;
					}
					setCursor( this, XC_crosshair);
					changedCursor( true);
					break;
				case DRAG:
					_mousex = float(event_x());
					_mousey = float(h() - event_y());
					break;
			}
			handled = true;
			break;
		case VM_SET_COLOR:
			switch (event)
			{
				case PUSH:
					break;
				case RELEASE:
				{
					double mx = double( event_x());
					double my = double( event_y());
					Pt3<float> pos = getObjPos( mx, my, 0.);
					parent()->segImage3D()->setLabelColor( pos);
					setViewMode( VM_NONE);
				}
				default:
					break;
			}
			break;
		default:
			break;
	}
	if ( handled)
		return 1;
	else
		return ModelView::handle( event);
}

//
//	ModelView overload
//	
void
MeshView::drawModel( GLenum rendMode)
{
	if ( !parent())		// no draw before initialize()
		return;

	Pt3<double> *cp = centerPt();
	Vector3d cv(  cp->x(), cp->y(), cp->z());
	Vector3d cpt = affine_transform_point( m_WorldMatrix, cv);

	glMatrixMode( GL_MODELVIEW);

	glPushMatrix();
	glLoadIdentity();
	glMultMatrixf( _transMatrix.glMatrixf());
	glMultMatrixf( _rotMatrix.glMatrixf());
	glTranslated( -cpt[0], -cpt[1], -cpt[2]);

	glPushMatrix();
	glMultMatrixd( m_WorldMatrix.transpose().data_block());

	// Set up the GL state for cube & image
	glPushAttrib(GL_LINE_BIT | GL_LIGHTING_BIT | GL_COLOR_BUFFER_BIT);
	glDisable(GL_LIGHTING);

	drawCube();

	if ( image())
		displayImg();

	glPopAttrib();		// restore lighting
	glPopMatrix();		// no m_WorldMatrix (Nifti)

	if ( segMesh() && parent()->showSegmentationToggle() && parent()->haveSegmentation())
	{
		// draw Segmentation Mesh
//		for ( uint wt=0; wt < 5; wt++)		// wt - wall type
//			if ( _showMesh[wt])
//				segMesh()->Display( wt);
		segMesh()->Display();
	}

	if ( parent()->showMeshToggle())
	{
		if ( parent()->registrationOn())
		{	// Registration transforms
			Pt3<float> cen =  mesh()->getBbox().center();
			for ( uint wt=0; wt < 5; wt++)		// wt - wall type
				if ( _showMesh[wt])
				{
DMV(				TransMatrix mv;)
DMV(				mv.modelGet();)			// gets current GL_MODELVIEW_MATRIX
DMV(				cout << "GL_MODELVIEW before glMultMatrixf( _modTransMatrix[wt].glMatrixf())" << endl;)
DMV(				cout << mv << endl;)

					glMultMatrixf( _modTransMatrix[wt].glMatrixf());
DMV(				mv.modelGet();)
DMV(				cout << "GL_MODELVIEW after glMultMatrixf( _modTransMatrix[wt].glMatrixf())" << endl;)
DMV(				cout << mv << endl;)

DMV(				mv.modelGet();)
DMV(				cout << "GL_MODELVIEW after glTranslated( center)" << endl;)
DMV(				cout << mv << endl;)
					glTranslatef( cen.x(), cen.y(), cen.z());
					glScalef( _modZoom, _modZoom, _modZoom);
					glMultMatrixf( _modRotMatrix[wt].glMatrixf());
DMV(				mv.modelGet();)
DMV(				cout << "GL_MODELVIEW after glMultMatrixf( _modRotMatrix[wt].glMatrixf())" << endl;)
DMV(				cout << mv << endl;)

					glTranslatef( -cen.x(), -cen.y(), -cen.z());
DMV(				mv.modelGet();)
DMV(				cout << "GL_MODELVIEW after glTranslated( -center)" << endl;)
DMV(				cout << mv << endl;)
				}
		}
		for ( uint wt=0; wt < 5; wt++)		// wt - wall type
			if ( _showMesh[wt])
				mesh()->Display( wt);
	}
	else if ( parent()->showRegMeshToggle())
	{
		for ( uint wt=0; wt < 5; wt++)		// wt - wall type
			if ( _showMesh[wt])
				regMesh()->Display( wt);
	}
	glPopMatrix();
}	// drawModel()

void
MeshView::displayImg()
{
	Pt3<float> pt[4];
	Vec3<double> clrBackground( 1., 1., 1.);
	Vec3<uint> sliceIndx = image()->GetSliceIndex();
	Pt3<float> minPt;
	Pt3<float> maxPt;
	bool posY = m_WorldMatrix[1][1] > 0.;
	if ( posY)		// Pig
	{
		minPt = _minPt;
		maxPt = _maxPt;
	}
	else			// Duncan - invert y
	{
		minPt.set( _minPt[0], _maxPt[1], _minPt[2]);
		maxPt.set( _maxPt[0], _minPt[1], _maxPt[2]);
	}

	if ( parent()->displayAxial())
	{
		// draw axial image, pts same z val
		pt[0].set( minPt.x(), minPt.y(), 0.F);
		pt[1].set( minPt.x(), maxPt.y(), 0.F);
		pt[2].set( maxPt.x(), maxPt.y(), 0.F);
		pt[3].set( maxPt.x(), minPt.y(), 0.F);
		float min = minPt.z();
		pt[0][2] = pt[1][2] = pt[2][2] = pt[3][2] = min + ((maxPt[2] - min) * 
			(float(sliceIndx[_imageAxes[0]]) / float(_imageSize[0])));
		if ( _lastSliceIndx[_imageAxes[0]] != sliceIndx[_imageAxes[0]])
			imgTex(0).SetImage( image()->GetDisplaySlice( 0).GetPointer());

		imgTex(0).Draw( clrBackground, pt);
	}

	if ( parent()->displaySagital())
	{
		// draw sagital image, pts same x val
		if ( posY)
		{
			pt[0].set( 0.F, maxPt.y(), maxPt.z());
			pt[1].set( 0.F, maxPt.y(), minPt.z());
			pt[2].set( 0.F, minPt.y(), minPt.z());
			pt[3].set( 0.F, minPt.y(), maxPt.z());
		}
		else
		{
			pt[0].set( 0.F, maxPt.y(), minPt.z());
			pt[1].set( 0.F, maxPt.y(), maxPt.z());
			pt[2].set( 0.F, minPt.y(), maxPt.z());
			pt[3].set( 0.F, minPt.y(), minPt.z());
		}
		float min = minPt.x();
		pt[0][0] = pt[1][0] = pt[2][0] = pt[3][0] = min + ((maxPt[0] - min) * 
			(float(sliceIndx[_imageAxes[1]]) / float(_imageSize[1])));
		if ( _lastSliceIndx[_imageAxes[1]] != sliceIndx[_imageAxes[1]])
			imgTex(1).SetImage( image()->GetDisplaySlice( 1).GetPointer());

		imgTex(1).Draw( clrBackground, pt);
	}
	
	if ( parent()->displayCoronal())
	{
		// draw coronal image, pts same y val
		if ( posY)			// Kludge here to get coronal right.  See 2014-12-01 log.
		{
			pt[0].set( minPt.x(), 0.F, maxPt.z());
			pt[1] = minPt;
			pt[2].set( maxPt.x(), 0.F, minPt.z());
			pt[3] = maxPt;
			float min = minPt.y();
			pt[0][1] = pt[1][1] = pt[2][1] = pt[3][1] = min + ((maxPt[1] - min) * 
				(float(sliceIndx[_imageAxes[2]]) / float(_imageSize[2])));
		}
		else
		{
			pt[0] = minPt;
			pt[1].set( minPt.x(), 0.F, maxPt.z());
			pt[2] = maxPt;
			pt[3].set( maxPt.x(), 0.F, minPt.z());
			float min = maxPt.y();
			pt[0][1] = pt[1][1] = pt[2][1] = pt[3][1] = min + ((minPt[1] - min) * 
				(float(sliceIndx[_imageAxes[2]]) / float(_imageSize[2])));
		}
		if ( _lastSliceIndx[_imageAxes[2]] != sliceIndx[_imageAxes[2]])
			imgTex(2).SetImage( image()->GetDisplaySlice( 2).GetPointer());

		imgTex(2).Draw( clrBackground, pt);
	}
	_lastSliceIndx = sliceIndx;
}

void
MeshView::drawCube()
{
	Pt3<float> minPt( _minPt);
	Pt3<float> maxPt( _maxPt);

	glColor4ub( WHITE_UP);

	// left side
	glBegin(GL_LINE_LOOP);
		glVertex3f( minPt.x(), minPt.y(), minPt.z());
		glVertex3f( minPt.x(), maxPt.y(), minPt.z());
		glVertex3f( minPt.x(), maxPt.y(), maxPt.z());
		glVertex3f( minPt.x(), minPt.y(), maxPt.z());
	glEnd();

	// right side
	glBegin(GL_LINE_LOOP);
		glVertex3f( maxPt.x(), minPt.y(), minPt.z());
		glVertex3f( maxPt.x(), maxPt.y(), minPt.z());
		glVertex3f( maxPt.x(), maxPt.y(), maxPt.z());
		glVertex3f( maxPt.x(), minPt.y(), maxPt.z());
	glEnd();

	// connect sides
	glBegin(GL_LINES);
		glVertex3f( minPt.x(), minPt.y(), minPt.z());
		glVertex3f( maxPt.x(), minPt.y(), minPt.z());
		glVertex3f( minPt.x(), maxPt.y(), minPt.z());
		glVertex3f( maxPt.x(), maxPt.y(), minPt.z());
		glVertex3f( minPt.x(), minPt.y(), maxPt.z());
		glVertex3f( maxPt.x(), minPt.y(), maxPt.z());
		glVertex3f( minPt.x(), maxPt.y(), maxPt.z());
		glVertex3f( maxPt.x(), maxPt.y(), maxPt.z());
	glEnd();
}

void
MeshView::rotateMesh()
{
	float ts = float(0.2);					// translation scale
	float rs = float(0.0002);				// rotation scale
	float zms = float(0.002);				// zoom scale

	for ( uint wt=0; wt < 5; wt++)		// wt - wall type
		if ( _showMesh[wt])
		{
			switch (_rotMode)
			{
				case XY_ROT:
					_modRotMatrix[wt].rotate((_mousex - _cenx) * rs, 'y');	// fixme!!! see 2013-08-27
					_modRotMatrix[wt].rotate(-(_mousey - _ceny) * rs, 'x');
					break;
				case Z_ROT:
					_modRotMatrix[wt].rotate(-(_mousex - _cenx) * rs, 'z');
					break;
				case ZOOM_ROT:	// this is now z-axis translate
				{
		//			cout << "_mousey = " << _mousey << ", _starty = " << _starty << endl;
					_modZoom += (_mousey - _startY[wt]) * zms;
					_startY[wt] = _mousey;
				}
					break;
				case XY_TRANS:
				{
					Vec3<float> v((_mousex - _startX[wt])*ts, (_mousey - _startY[wt])*ts, 0.F);
					Vec4<float> v4( v);
					v4 = _rotMatrix * v4;			// need to get trans rotated first.
					v.set( v4.x(), v4.y(), v4.z());
					_modTransMatrix[wt] *= (translation3Dt(v));
					_startX[wt] = _mousex;
					_startY[wt] = _mousey;
				}
					break;
				case NO_ROT:
				default:
					break;
			}
		}
	parent()->initRegMesh();		// apply rotMatrices to regMesh
	parent()->redraw();			// so redraw ImgView's also
}

void
MeshView::rotateMeshTimerCB( void* v)
{
	fltk::repeat_timeout( ROTATE_INTERVAL, rotateMeshTimerCB, v);
	((MeshView *)v)->rotateMesh();
}
