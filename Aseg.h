//
//	Aseg.h	-- Program for segmenting the atrium (human, dog, pig) from CT 3D images
//
//	18-nov-11  Barry Branham
//	Modified:
//	05-dec-11  bhb	add setROI(), _ROI, roi()
//	16-dec-11  bhb	add IntensityFilter callbacks
//	19-dec-11  bhb	add load/saveIntensity(), _settings, _intensitySettingsLoaded
//	20-dec-11  bhb	add SetBubbles, setViewMode, setBubblesOK, setBubblesCancel
//	27-dec-11  bhb	change LabelType to LabelTypeUS because of conflict w/ fltk
//	14-mar-12  bhb	add _snakeRunning, _haveSegmentation
//	11-apr-12  bhb	add updateFibrosisThreshold(), FibrosisOkCB(), FibrosisApplyCB(),
//						setThreshImage(), showThreshold()
//	18-jul-12  bhb	add generateWallMeshes()
//	15-jul-13  bhb	add loadPic3Model()
//	17-oct-13  bhb	add deletePic3Model()
//	25-oct-13  bhb	add registerPic3toggle()
//	29-oct-13  bhb	add setSegmentationImage()
//	04-nov-13  bhb	add ThresholdFilterCancel()
//	05-nov-13  bhb	rename enum DISPLAY_MENU/REG_PIC3 to REG_MESH, rename 
//					registerPic3toggle() to registerMeshToggle()
//	19-nov-13  bhb	remove setSegmentationImage()
//	25-nov-13  bhb	add invertMeshY()
//	03-dec-13  bhb	removed load/saveThreshold(), now in ThresholdFilter
//	04-dec-13  bhb	add SetDrawingColorLabel() & setColor(),
//					change meshColorOkCB() to colorOkCB(), & meshColorOK() to colorOK()
//	06-dec-13  bhb	add load/saveSegmentationMesh()
//	09-dec-13  bhb	add resizeMesh()
//	22-apr-14  bhb	add _modelReg, registerModel()
//	06-jun-14  bhb	add registrationOn()
//	11-jun-14  bhb	change _transMesh name to _regMesh
//	20-jan-14  bhb	add showSnake()
//	20-jun-14  bhb	remove invertMeshY()
//	13-oct-14  bhb	add ACWE segmentation: _segmentationType, enum SegmentationType
//	14-oct-14  bhb	remove _haveSegmentation bool, use snapImage()->IsSegmentationLoaded()
//	15-oct-14  bhb	rename setThreshImage() to setLabelImage()
//	17-nov-14  bhb	remove _pic3Model & related access funcs
//	19-dec-14  bhb	remove loadPic3Model.  No need for.
//	23-mar-15  bhb	change ACWE segmentation to GeodesicActiveContour segmentation
//	02-apr-15  bhb	renamed setMeshObject() to setImgViewMesh(), add initRegMesh()
//	07-apr-15  bhb	renamed showThreshold() to showSpeed()
//	09-apr-15  bhb	add showMesh(), showMeshToggle(), updateMeshToggles()
//	10-apr-15  bhb	add _segMesh, updateSegMesh()
//	16-apr-15  bhb	add showRegMeshToggle()
//	29-may-15  bhb	renamed load/saveSegmentationMesh() to load/saveMesh()
//	09-jun-15  bhb	add segmentationCancel()
//	06-oct-15  bhb	delete doSegment2d(), change doSegmentation() to have bool 2d arg,
//					add bool _segment2D
//	13-oct-15  bhb	add setSnakeImage2D()
//	12-nov-15  bhb	remove GeodesicActiveContour seg (keep in Snake seg), _segmentationType &
//					enum, _GeoActContParams, selectSegmentationType()
//	17-nov-15  bhb	add _MeshWindowUI, showMeshWindow()
//	08-jun-17  bhb	add _seeds
//	13-jun-17  bhb	add loadSeeds(), saveSeeds()
//	14-jun-17  bhb	modify to use SegParameters instead of renamed SnakeParameters
//	12-jul-17  bhb	remove 2D segmentation operation
//	31-aug-17  bhb	rename _snapImage to _segImage3D, ref segImage3D(), remove seed stuff
//	22-sep-17  bhb	add showBubblesToggle(), showBubbles()
//	
#ifndef _ASEG_H
#define _ASEG_H

#include "UI/AsegUI.h"
#include <GL/gl.h>						// so Pt.h has GL #defines
#include "Mesh/MeshObject.h"			// #includes SegImage3D.h
#include "Mesh/MeshOptions.h"
#include <ITK/ItkImage3D.h>
#include "itkDefaultDynamicMeshTraits.h"
#include <Geom/Sphere.h>
#include <Geom/Polygon.h>
#include <ITK/ColorLabelTable.h>
#include <ITK/ImageCoordinateGeometry.h>
#include <ITK/EdgePreprocessingSettings.h>
#include <ITK/ThresholdSettings.h>
#include <CXlib/StartOps.h>
#include <string>
#include <vector>
#include <pthread.h>

class SegImage3D;
class GlFont;
class ImageIntensity;
class ThresholdFilter;
class EdgeFilter;
class SetBubbles;
class SegParameters;
class SegmentationDlog;
class ImgWindowUI;
class MeshWindowUI;
class AddWallUI;
class FibrosisHistogram;
class ImageInfo;
class ColorLabelDlog;
class ModelReg;

namespace itk {
  template <class TInputImage, class TOutputImage> class RegionOfInterestImageFilter;
};

namespace fltk
{
//	class fltk::FileChooser;
	class ItkProgress;
}

enum CoverageModeType 
{
  PAINT_OVER_ALL,
  PAINT_OVER_ONE,
  PAINT_OVER_COLORS
};

//
class Aseg : public AsegUI
{
	public:
		Aseg( int x, int y, int w, int h, const char *title);
		~Aseg();

		enum { Dimension = 3 };
		typedef itk::ImageRegion<3>	RegionType;
		typedef itk::Image< LabelTypeUS, Dimension>		LabelImageType;
		typedef itk::Image< LabelTypeUS, 2>				LabelImage2dType;
		typedef itk::Index<Dimension>					IndexType;
		typedef itk::RegionOfInterestImageFilter< LabelImageType, LabelImageType>
														RoiFilterType;

		enum FILE_MENU	{ DO_STARTOPS, LOAD_IMAGES, LOAD_ROI, LOAD_BUBBLES,
							LOAD_SEGMENTATION, LOAD_MESH, LOAD_WALLS, DIV1, 
							SAVE_ROI, SAVE_BUBBLES, SAVE_SEGMENTATION, 
							SAVE_INIT_MESH, SAVE_SEG_MESH, SAVE_WALLS, SAVE_FIBROSIS, 
							SAVE_STARTOPS};
		enum EDIT_MENU	{ THRESH_PARM, EDGE_PARM, SET_SEEDS, SET_BUBBLES,
							SEG_PARMS, CLEAR_SEGMENTATION, SET_COLOR, 
							DIV2, ADD_WALL, EDIT_WALL, DELETE_WALL, COPY_PREV_WALLS,
							COPY_NEXT_WALLS, DIV3, DRAW_CIRCLE, DELETE_CIRCLE, INTENSITY};
		enum DISPLAY_MENU	{ SET_ROI, RESET_ROI, RESET_3D, DIV4, REG_MESH, RESET_REG, DIV5, 
								SHOW_BUBBLES, SHOW_MESH, SHOW_REGMESH, SHOW_SPEED, SHOW_SHAPE, 
								SHOW_SNAKE, SHOW_SEG, SHOW_FIB, SHOW_PIXVAL, SHOW_XY};
		enum OPER_MENU	{ SEGMENT, REG_MODEL, SEG_VOLUME, ALL_SEG_VOLUME, 
							FIBROSIS};
		enum StartOpKeys	{ LoadImages, LoadRoi, LoadWalls, LoadBubbles,
								LoadSegmentation, LoadMesh};
		
		// access
		//
		fltk::Window *		window()				{ return _window;	}
		fltk::StatusBarGroup *statusBar()			{ return _statusBar;	}
		ItkImage3D *		image()					{ return _image;	}
		ItkImage3D *		mainImage()				{ return _mainImage[_imageTime];	}
		SegImage3D *		segImage3D()			{ return _segImage3D[_imageTime];	}
		LabelImageType::Pointer segImage()			{ return _segImage;	}
		ImageCoordinateGeometry & getGeometry()		{ return _ImageCoordinateGeometry;	}
		uint				imageAxis( uint i)		{ return _imageAxes[i];	}
		ImgView *			imgView( uint i)		{ return _imgView[i];	}
		MeshView *			meshView()				{ return _meshView;	}
		GlFont *			font()					{ return _font;	}
		std::string &		currentDirectory()		{ return _currentDirectory;	}
		std::string &		fileName()				{ return _fileName;	}
		Vec3<uint>			cursor()				{ return _cursor;	}
		MeshObject *		mesh()					{ return &_mesh;	}
		MeshObject *		regMesh()				{ return _regMesh;	}
		MeshObject *		segMesh()				{ return &_segMesh;	}
		RegionType &		mainROI()				{ return _mainROI;	}
		RegionType &		segROI()				{ return _segROI;	}
		void				setEdgeSettings( EdgePreprocessingSettings &settings)
							{ _edgeSettings = settings;	_edgeSettingsLoaded = true;}
		EdgePreprocessingSettings & getEdgeSettings()		{ return _edgeSettings;	}
		bool				haveEdgeSettings()	{ return _edgeSettingsLoaded;	}
		void				setThresholdSettings( ThresholdSettings &settings)
							{ _threshSettings = settings;	_thresholdSettingsLoaded = true;}
		ThresholdSettings & getThresholdSettings()	{ return _threshSettings;	}
		bool				haveThresholdSettings()	{ return _thresholdSettingsLoaded;	}
		bool				haveBubbles()			{ return _bubbles.size() > 0;	}
		MeshOptions &		meshOptions()			{ return _meshOptions;	}
		std::vector<Sphere<uint> > & bubbles()		{ return _bubbles;	}
		std::vector<IndexType> & seeds()			{ return _seeds;	}
		fltk::ItkProgress *	progress()				{ return _progress;	}
		bool				snakeActive()			{ return _snakeActive;	}
		bool				snakeRunning()			{ return _snakeRunning;	}
		ColorLabelTable	*	GetColorLabelTable()	{ return &_ColorLabelTable;	}
		LabelTypeUS			SetDrawingColorLabel( LabelTypeUS c)	{ _DrawingColorLabel = c;}
		LabelTypeUS			GetDrawingColorLabel()	{ return _DrawingColorLabel;}
		SetBubbles *		bubblesUI()				{ return _setBubbles;	}
		
		SegmentationDlog *	segmentationDlog()			{ return _SegmentationDlog;	}
		bool				haveSegmentation()		{ return imgView(0)->labelImage() != 0;}
		fltk::LightButton *	registerMeshToggle()	{ return _registerMeshToggle;	}
		bool				registrationOn()		{ return _registerMeshToggle->value();	}
		bool				showSnakeToggle() 		{ return _showSnakeToggle->value();}
		bool				showSegmentationToggle() { return _showSegmentationToggle->value();}
		bool				showBubblesToggle()		{ return _showBubblesToggle->value();	}
		bool				showMeshToggle()		{ return _showMeshToggle->value();	}
		bool				showRegMeshToggle()		{ return _showRegMeshToggle->value();	}
		bool				displayAxial()			{ return _showAxial; }
		bool				displaySagital()		{ return _showSagital; }
		bool				displayCoronal()		{ return _showCoronal; }
		bool				displayMesh(uint m)		{ return _MeshDisplayButton[m]->value();}

		// actions

		// status bar
		void				setStatus( const char *msg)
							{ _statusBar->set( msg, fltk::StatusBarGroup::SBAR_LEFT); }
		void				setStatus( const char *msg, int n)
							{ _statusBar->set( fltk::StatusBarGroup::SBAR_LEFT, msg, n); }
		void				setStatus( const char *msg, unsigned int n)
							{ _statusBar->set( fltk::StatusBarGroup::SBAR_LEFT, msg, n); }
		void				setStatus( const char *msg, float n)
							{ _statusBar->set( fltk::StatusBarGroup::SBAR_LEFT, msg, n); }
		void				clearStatus()
							{ _statusBar->set( 0, fltk::StatusBarGroup::SBAR_LEFT); }
				
		// called from ImgView
		void			setRoi( RegionType &roi);
		void			setCrosshairs( Pt3<float> &pt);
		uint			getSlice( uint dim)	
						{ return _sliceScrollbar[dim]->minimum()-_sliceScrollbar[dim]->value();}
		void			setSlice( uint dim, double val)
						{ _sliceScrollbar[dim]->value(_sliceScrollbar[dim]->minimum() - val);}

		// called from ThresholdFilter
		void			preprocessingPreview( bool preview);

		// called from SegmentationDlog
		void			restartSegmentation();
		friend void		SnakeIdleFunction( void *userData);
		void			startSegmentation();
		void			stopSegmentation( bool updateMesh=true);
		void			stepSegmentation();

		void			updateSegmentationData();
		LabelTypeUS		DrawOverLabel( LabelTypeUS iTarget);
		void			segmentationOK();
		void			segmentationCancel();
		void			intensityChanged();
		void			intensityOk();
		void			intensityCancel();
		void			activateEditMenu();
		void			redraw();
		void			initRegMesh();
		void			winSliceChange( double val);			// public for ImgWindowUI

		// public for MeshWindowUI
		virtual void	showAxial();
		virtual void	showSagital();
		virtual void	showCoronal();
		virtual void	showMesh( uint wt);
		virtual void	rotate90(unsigned int dim);		// MeshView buttons
		virtual void	reset3D();						// MeshView button
		
		pthread_t 		_thread;
		bool			_segUpdateDone;		// public, called from runSegmentation thread func

	private:

		ItkImage3D *				_image;	// _mainImage[t] or _segImage3D[t] depending on ROI
		std::vector<ItkImage3D *>	_mainImage;		// vector for different times
		std::vector<SegImage3D *>	_segImage3D;	// "
		LabelImageType::Pointer		_segImage;		// initial seg image, from .gipl
		LabelImageType::Pointer		_segROIImage;	// above or ROI version

		ImageCoordinateGeometry		_ImageCoordinateGeometry;
		uint						_imageAxes[3];
		Vec3<uint>					_cursor;		// in image (pixel) coords

		MeshObject					_mesh;			// loaded mesh
		MeshObject *				_regMesh;		// _mesh registered
		MeshObject					_segMesh;		// segmentation mesh
		uint						_imageTime;
		uint						_wallImgViewId;
		
		ColorLabelTable				_ColorLabelTable;

		RegionType					_mainROI;		// total region main IRIS image, pixels
		RegionType					_segROI;  		// Region of interest for segmentation
		EdgePreprocessingSettings	_edgeSettings;
		bool						_edgeSettingsLoaded;
		ThresholdSettings			_threshSettings;
		bool						_thresholdSettingsLoaded;
		MeshOptions					_meshOptions;
		ImageIntensity *			_intensityFilter;		// ImageIntensity UI
		ThresholdFilter *			_thresholdFilter;		// ThresholdFilter UI
		EdgeFilter *				_edgeFilter;			// EdgeFilter UI
		FibrosisHistogram *			_fibrosisHistogram;		// FibrosisHistogram analysis & UI
		std::vector<GreyType>		_fibrosisPixels;		// Fibrosis histogram pixels

		SetBubbles *				_setBubbles;			// Set bubbles UI
		std::vector<IndexType>		_seeds;
		std::vector<Sphere<uint> >	_bubbles;
		
		SegParameters *				_SegParameters;			// SegParametersUI is base class
		LabelTypeUS					_DrawingColorLabel;		// Color label to draw polygons
		SegmentationDlog *			_SegmentationDlog;		// Segmentation Dialog
		ModelReg *					_modelReg;
		ImgWindowUI *				_ImgWindowUI;
		MeshWindowUI *				_MeshWindowUI;
		AddWallUI *					_AddWallUI;
		ImageInfo *					_imageInfo;
		ColorLabelDlog *			_ColorLabelDlog;
		bool						_snakeActive;		// for doing snake, MeshObject checks
		bool						_snakeRunning;		// for seg Dlog, thread operations
		StartOps					_newStartOps;
		fltk::ItkProgress *			_progress;

		bool						_showAxial;
		bool						_showSagital;
		bool						_showCoronal;
		
		fltk::FileChooser *			_fileChooser;
		GlFont *					_font;
		std::string					_currentDirectory;
		std::string					_fileName;
		
		// Overloaded UI functions, can be private
		// File menu
		virtual	void	doStartOps();
		virtual void	saveStartOps();
		virtual bool	loadImages( const char *imgFileName = 0);
		virtual bool	loadROI( const char *fileName = 0);
		virtual void	saveROI();
		virtual bool	loadBubbles( const char *fileName = 0);
		virtual void	saveBubbles();
		virtual bool	loadSegmentation( const char *fileName = 0);
		virtual void	saveSegmentation();
		virtual bool	loadMesh( const char *fileName = 0);
		virtual bool	saveMesh( uint type);
		virtual bool	loadWalls( const char *fileName);
		virtual void	saveWalls();
		virtual void	saveFibrosis();
		virtual void	quit();

		// Edit menu
		virtual void	setEdgeSettings();
		virtual void	setThresholdSettings();
		virtual void	setBubbles();
		virtual void	editSegmentationParameters();
		virtual bool	resetSegmentation();
		virtual void	clearSegmentation();
		virtual void	setColor();
		virtual void	addWall( uint w);
		virtual void	editWall();
		virtual void	deleteWall();
		virtual void	copyWalls( uint n);
		virtual void	drawCircle();
		virtual void	deleteCircle();
		virtual void	setIntensity();

		// Display menu
		virtual void	setROI();
		virtual void	resetROI();
		virtual void	registerMesh();
		virtual void	resetRegistration();
		virtual void	showBubbles();
		virtual void	showMesh();
		virtual void	showRegisteredMesh();
		virtual void	showSpeed();
		virtual void	showInitialShape();
		virtual void	showSnake();
		virtual void	showSegmentation();
		virtual void	showFibrosisThreshold();
		virtual void	showPixelValue();
		virtual void	showXY();

		// Analysis menu
		virtual void	doSegmentation();
		virtual void	registerModel();
		virtual bool	getSegmentationVolume();
		virtual void	getAllSegmentationVolumes();
		virtual void	analyzeFibrosis( uint w);

		// other UI
		virtual void	sliceChange();				// ImgView slice sliders
		virtual void	setTime();
		virtual void	showImgWindow( uint i);
		virtual void	showMeshWindow();
		virtual void	showInfo();

		virtual void	displayMenuHelp();
		virtual void	analysisMenuHelp();

		// more private
		void			initROI();
		void			initSegImage();
		bool			roiChanged()	{ return !(_mainROI == _segROI);	}
		template< typename ImgType, typename PixType> 
		void			GetMinMax( ImgType *img, PixType &min, PixType &max, const char *msg=0);

		void			activateMenus();
		void			setImage( GreyImageWrapper3D *wrapper, 
							std::vector<float> spacing);
		void			setSpeedImage( SpeedImageWrapper3D *wrapper);
		void			setSpeedImage2D( SpeedImageWrapper *wrapper);
		void			setInitShapeImg( LabelImageWrapper3D *wrapper);
		void			setSnakeImage( LevelSetImageWrapper3D *wrapper);
		void			setLabelImage( LabelImageWrapper3D *wrapper);
		void			setImgViewMesh();
		void			setViewMode( ImgView::ViewMode m);
		void			setDisplayType( ImgView::DisplayType dt);
		void			displayRadio( fltk::LightButton *b);
		void			radioDisplay( fltk::LightButton *button);
		void			centerChildWindow( fltk::Window *childWin);
		void			setCursor( int ncur);
		void			initSliders();
		void			initWinSlider();
		void			setWinSlice( double val);

		void			snakeUpdate( bool updateMesh=true);
		void			updateMesh( bool showProg = false);
		void			updateSegMesh( bool showProg = false);
		void			updateMeshToggles();
		void			updateFibrosisThreshold();
		void			generateWallMeshes();
		void			ThresholdFilterApply();
		void			ThresholdFilterCancel();
		void			EdgeFilterApply();
		void			EdgeFilterCancel();
		void			setBubblesOk();
		void			shortImageName( std::string &iname, std::string &sname);
		void			colorOK();
		
		static void	ThresholdFilterOkCB( fltk::Widget* o, void* callData);
		static void	ThresholdFilterApplyCB( fltk::Widget* o, void* callData);
		static void	ThresholdFilterCancelCB( fltk::Widget* o, void* callData);

		static void	EdgeFilterOkCB( fltk::Widget* o, void* callData);
		static void	EdgeFilterApplyCB( fltk::Widget* o, void* callData);
		static void	EdgeFilterCancelCB( fltk::Widget* o, void* callData);
		
		static void	setBubblesOkCB( fltk::Widget* o, void* callData);
		static void	setBubblesCancelCB( fltk::Widget* o, void* callData);

		static void	editParametersOkCB( fltk::Widget* o, void* callData);
		static void	editParametersCancel( fltk::Widget* o, void* callData);

		static void SegmentationOkCB( fltk::Widget* o, void* callData);
		static void SegmentationCancelCB( fltk::Widget* o, void* callData);

		static void AddWallOkCB( fltk::Widget* o, void* callData);
		static void AddWallCancelCB( fltk::Widget* o, void* callData);
		
		static void FibrosisOkCB( fltk::Widget* o, void* callData);
		static void FibrosisApplyCB( fltk::Widget* o, void* callData);

		static void intensityOkCB( fltk::Widget* o, void* callData);
		static void intensityCancelCB( fltk::Widget* o, void* callData);

		static void colorOkCB( fltk::Widget* o, void* callData);
};
#endif