//
//  ImgView.h  -- ImgView class
//
//  18-nov-11  bhb  from Getpic3 ImgView.h
//  Modified:
//	20-dec-11  bhb	delete drawSpeedImage() stub, add drawBubbles()
//	11-apr-12  bhb	add _threshImage, DT_THRESH
//	01-aug-12  bhb	remove getRoi(), initRoi(), incorporate into resetsRoi()
//	16-jul-13  bhb	add Pic3Model, what to display slice curve
//	06-aug-13  bhb	add _modTransMatrix, _modRotMatrix, _centerPt for Pic3Model registration
//	29-oct-13  bhb	add _segImage, _segTex, setSegmentationImage(), segImage(), segTex() &
//					DT_SEG
//	15-nov-13  bhb	add _mesh
//	19-nov-13  bhb	mesh display working, remove DT_SEG, _segImage, _segTex,
//					setSegmentationImage(), segImage(), segTex().
//	25-nov-13  bhb	add invertMeshY(), _invertMeshY
//	24-apr-14  bhb	add VM_DRAW_CIRCLE, centerPt(), _radius, radius()
//	29-apr-14  bhb	add getPixel()
//	20-jun-14  bhb	remove _invertMeshY, fixed Sag, Cor mesh invert prob (kludge)
//	13-oct-14  bhb	add poly()
//	15-oct-14  bhb	change 'thresh'Image etc to 'label' & DT_THRESH to DT_LABLE so generic.
//	16-apr-15  bhb	add _regMesh
//	24-sep-15  bhb	change DisplayImg() to allow DT_MAIN or DT_SPEED with DT_LABEL/FIB
//	13-oct-15  bhb	add _snakeImage2D
//	28-oct-15  bhb	add _speedImage2D
//	08-jun-17  bhb	change ImgViewMode to ViewMode, add VM_SEEDS
//	12-jul-17  bhb	remove 2D operation
//	07-sep-17  bhb	change _speedImage2D to _initSnakeImg
//	08-sep-17  bhb	put back _speedimage2D for Edge & Threshold filters
//	13-sep-17  bhb	rename _initSnakeImg to _initShapeImg
//	
#ifndef IMGVIEW_H
#define IMGVIEW_H

#include <Plot/OpenGLTexture.h>
#include <stdio.h>
#include <stdlib.h>
#include <fltk/gl.h>
#include <fltk/GlWindow.h>
#include <Geom/Pt.h>
#include <Geom/RubberBox.h>
#include <Geom/TransMatrix.h>
#include <Geom/Ellipse.h>			// Circle()
#include <fltk/Cursor.h>
#include <Fltk/XyTip.h>
#include <ITK/ImageCoordinateTransform.h>

class Aseg;
class GreyImageWrapper3D;
class SpeedImageWrapper3D;
class SpeedImageWrapper;
class LevelSetImageWrapper3D;
class LabelImageWrapper3D;
class ColorLabelTable;
class MeshObject;

#define DT_MAIN		0x1
#define DT_SPEED	0x2
#define DT_SNAKE	0x4
#define DT_LABLE	0x8
#define DT_FIB		0x10
#define DT_INIT		0x20
#define DT_BUBBLES	0x40

class   ImgView : public fltk::GlWindow
{
	public:
		enum	ViewMode { VM_NONE, VM_ZOOM, VM_DRAW_WALL, VM_EDIT_WALL, VM_DRAW_CIRCLE, 
							VM_DELETE_WALL, VM_REVERSE_WALL, VM_DISTANCE, VM_DIST_OFF, 
							VM_DETAIL, VM_POINT, VM_CROSS_HAIR, VM_BUBBLE, VM_SEEDS, 
							VM_PIXVAL};
//		enum	DisplayType { DT_MAIN, DT_SPEED, DT_SNAKE, DT_LABLE, DT_FIB};
		typedef	unsigned int	DisplayType;
		
		ImgView( int x, int y, int w, int h);
		~ImgView();

		// access
		void		setParent( Aseg *aseg)			{ _Aseg = aseg;	}
		Aseg *		parent()						{ return _Aseg;	}
		void		setId( uint id)					{ _id = id;	}
		uint		getId()							{ return _id;	}
		void		setImage( GreyImageWrapper3D *img, std::vector<float> spacing);
		void		setSpeedImage( SpeedImageWrapper3D *img); //, const std::vector<float> &spacing);
		void		setSpeedImage2D( SpeedImageWrapper *img)	{ _speedImage2D = img;	}
		void		setInitShapeImg( LabelImageWrapper3D *img)	{ _initShapeImg = img;	}
		void		setSnakeImage( LevelSetImageWrapper3D *img)	{ _snakeImage = img;	}
		void		setLabelImage( LabelImageWrapper3D *img)	{ _labelImage = img;	}
		GreyImageWrapper3D *	image()				{ return _image;	}
		SpeedImageWrapper3D *	speedImage()		{ return _speedImage;	}
		SpeedImageWrapper *		speedImage2D()		{ return _speedImage2D;	}
		LabelImageWrapper3D *	initShapeImg()		{ return _initShapeImg;	}
		LevelSetImageWrapper3D *snakeImage()		{ return _snakeImage;	}
		LabelImageWrapper3D *	labelImage()		{ return _labelImage;	}
		void		mesh( MeshObject *m)			{ _mesh = m;	}
		MeshObject *mesh()							{ return _mesh;}
		void		regMesh( MeshObject *m)			{ _regMesh = m;	}
		MeshObject *regMesh()						{ return _regMesh;}
		void		segMesh( MeshObject *m)			{ _segMesh = m;	}
		MeshObject *segMesh()						{ return _segMesh;}
		void		transMatrix( TransMatrix *tm)	{ _modTransMatrix = tm;	}
		void		rotMatrix( TransMatrix *rm)		{ _modRotMatrix = rm;	}
		Pt2<float>	centerPt()						{ return _centerPt;	}
		void		centerPt( Pt2<float> *cp)		{ _centerPt = *cp;	}
		float		radius()						{ return _radius;	}
		
		void        setMode( ViewMode m);
		ViewMode 	getMode()       				{ return _mode; }
		void		setDisplayType( DisplayType dt)	{ _displayType = dt;	}
		DisplayType	getDisplayType()				{ return _displayType;	}
		OpenGLTexture &imgTex()						{ return _imgTex;  }
		OpenGLTexture &snakeTex()					{ return _snakeTex;  }
		OpenGLTexture &labelTex()					{ return _labelTex;  }
		RubberBox *	resizeBox()						{ return &_resizeBox;   }
		int &		rbIndex()						{ return _resizeBoxIndex;	}
		float		rbTol()							{ return _resizeBoxTol;	}
		XyTip *		xyTip()							{ return _xyTip;	}
		float		getZoom()						{ return m_ViewZoom;	}
		Polygon<Pt2<float>, float> *	poly()		{ return &_poly;	}
		
		// actions

		void		reset();										// but need flush()
		void		resetRoi();
		void		showXYoff()	{ if ( xyTip()!=NULL) xyTip()->hideXY();	}
		void		showDistance( bool v)	{ _mode = (v ? VM_DISTANCE : VM_DIST_OFF);	}
		void		showDetail()	{}
		void		clearPoly()		{ _poly.clear();	}
		
	protected:

	private:
		Aseg *					_Aseg;					// parent app
		uint					_id;					// our display dimension
		GreyImageWrapper3D *	_image;					// our image
		SpeedImageWrapper3D *	_speedImage;			// filtered image to segment
		SpeedImageWrapper *		_speedImage2D;
		LabelImageWrapper3D *	_initShapeImg;
		LevelSetImageWrapper3D *_snakeImage;
		LabelImageWrapper3D *	_labelImage;			// fibrosis/seg image
		MeshObject *			_mesh;
		MeshObject *			_regMesh;
		MeshObject *			_segMesh;
		TransMatrix *			_modTransMatrix;
		TransMatrix *			_modRotMatrix;
		Pt2<float>				_centerPt;
		float					_radius;
		
		ImageCoordinateTransform _ImageToDisplayTransform;
		ImageCoordinateTransform _DisplayToImageTransform;
		ImageCoordinateTransform _DisplayToAnatomyTransform;
		uint				_imageAxes[3];
		uint				_sliceSize[3];
		Vec3<float>			_sliceSpacing;
		float				_DisplayAxisPosition;
		
		OpenGLTexture 		_imgTex;				// current texture image
		OpenGLTexture		_snakeTex;				// active Segmentation slice texture
		OpenGLTexture		_labelTex;				// fibrosis/seg slice texture
		
		ViewMode 			_mode;					// input() mode
		DisplayType			_displayType;
		RubberBox 			_resizeBox;				// for selecting zoom region
		int					_resizeBoxIndex;		// current resizeBox edge index
		float				_resizeBoxTol;
		Pt3<float> *    	_editPt;				// current pt for dragging.
		Pt3<float> *    	_tmpPt;					// temp point for adding point
		Polygon<Pt2<float>, float> _poly;
		XyTip *				_xyTip;
		bool				_init;

		float				m_ViewZoom;
		Vec2<float>			m_ViewPosition;

		// actions

		void		initGraphic();
		
		virtual void draw();
		virtual void draw_overlay();
		virtual int handle( int event);

		void		DisplayImg();
		void    	drawOverlayGraphics();
		void		drawMesh( MeshObject *mesh);
		void		drawScale();
		void		drawCrosshairs();
		void		drawAxisLabels();
		void		drawSliceNumber();
		void		drawBubbles();
		Pt3<float>  MapSliceToImage( const Pt3<float> &pt);
		Pt3<float>	MapImageToSlice( const Pt3<float> &imgPt);
		Pt3<float>	MapSliceToWindow( const Pt3<float> &xSlice);
		Vec3<float> MapWindowToSlice( const int x, const int y);
		Pt3<float>  mouseLoc( int x, int y);	// returns the image coords at x,y,z

		void		updateRoi( int x, int y);
		Vec4<float> getSlicePlane();
		void		showXY( Pt3<float> &pt);
		void		setCursor( Vec3<uint> &cursor, Pt3<float> &pt);
		float		getPixel( Pt3<float> &pt);
};
#endif		//  IMGVIEW_H
