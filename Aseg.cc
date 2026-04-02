//
//	Aseg.cc	-- Program for segmenting the atrium (human, dog, pig) from CT 3D images
//
//	18-nov-11	Barry Branham
//	Modified:
//	xx-dec-11  bhb	lots of work
//	xx-jan-12  bhb	ditto
//	31-jan-12  bhb	add OnITKProgressEvent(), centerChildWindow()
//	02-feb-12  bhb	add initSegImage()
//	10-feb-12  bhb	fix mesh slice update, redraw after segmentation done
//	01-mar-12  bhb	Modified AsegUI to have 'File Edit Display Operation' menu headings.
//	12-mar-12  bhb	added Edit/Reset Segmention
//	14-mar-12  bhb	fixed up segmentation operation, add _snakeRunning, _haveSegmentation
//	18-jun-12  bhb	loadImages(): check for ".lst" in filename and use RFT_LIST if present.
//	18-jul-12  bhb	add generateWallMeshes()
//	15-jul-13  bhb	add loadPic3Model()
//	17-oct-13  bhb	add deletePic3Model()
//	25-nov-13  bhb	add invertMeshY()
//	03-dec-13  bhb	removed load/saveThreshold(), now in ThresholdFilter
//	04-dec-13  bhb	add SetDrawingColorLabel() & setColor() (V1.2.0)
//	06-dec-13  bhb	add Aseg::load/saveSegmentationMesh() (V1.2.1)
//	09-dec-13  bhb	add resizeMesh() & resizeMesh( float)
//	03-jun-14  bhb	fixed loadPic3Model() scaling
//	11-jun-14  bhb	change _transMesh name to _regMesh
//	20-jan-14  bhb	add showSnake()
//	20-jun-14  bhb	remove invertMeshY()
//	13-oct-14  bhb	add ACWE segmentation: _segmentationType, enum SegmentationType
//	14-oct-14  bhb	remove _haveSegmentation bool, use snapImage()->IsSegmentationLoaded()
//	19-dec-14  bhb	remove loadPic3Model.  No need for.
//	23-mar-15  bhb	V1.3.0, change ACWE segmentation to GeodesicActiveContour segmentation
//	31-mar-15  bhb	V1.3.0, fixed mesh center in redraw()
//	02-apr-15  bhb	renamed setMeshObject() to setImgViewMesh(), add initRegMesh()
//	09-apr-15  bhb	add showMesh(), showMeshToggle(), updateMeshToggles()
//	10-apr-15  bhb	add _segMesh, updateSegMesh()
//	29-may-15  bhb	renamed load/saveSegmentationMesh() to load/saveMesh()
//	09-jun-15  bhb	add segmentationCancel()
//	13-oct-15  bhb	add setSnakeImage2D()
//	12-nov-15  bhb	remove GeodesicActiveContour seg (keep in Snake seg)
//	12-nov-15  bhb	remove GeodesicActiveContour seg (keep in Snake seg), _segmentationType &
//					enum, _GeoActContParams, selectSegmentationType()
//	17-nov-15  bhb	add _MeshWindowUI, showMeshWindow()
//	13-jun-17  bhb	add loadSeeds(), saveSeeds()
//	12-jul-17  bhb	remove 2D segmentation operation
//	31-aug-17  bhb	rename _snapImage to _segImage3D, ref segImage3D(), remove seed stuff
//	22-sep-17  bhb	add showBubblesToggle(), showBubbles()
//
#define WIN_TITLE	"Aseg 1.3.1"

#include "Aseg.h"
#include "SegImage3D.h"
#include "ImgView.h"
#include "ImageIntensity.h"
#include "ThresholdFilter.h"
#include "EdgeFilter.h"
#include "FibrosisHistogram.h"
#include "SetBubbles.h"
#include "LevelSet/SegParameters.h"
#include "SegmentationDlog.h"
#include "ModelReg.h"
#include <ModelsFl/Surface.h>
#include "UI/ImgWindowUI.h"
#include "UI/MeshWindowUI.h"
#include "UI/AddWallUI.h"
#include "UI/ColorLabelDlog.h"

#include <ITK/ItkImageIO3D.h>
#include <ITK/SegmentationROISettings.h>
#include <PlotFl/GlFont.h>
#include <Fltk/Fltk.h>
#include <ItkFltk/ItkProgress.h>
#include <CXlib/Timer.h>
#include <CXlib/FileOps.h>
#include <UIFl/ImageInfo.h>
#include <fltk/run.h>
#include <fltk/ask.h>
#include <fltk/FileChooser.h>
#include <fltk/Cursor.h>
#include <X11/cursorfont.h>
#include "itkRegionOfInterestImageFilter.h"
#include <string.h>
#include <sstream>
#include <errno.h>
#include <algorithm>			// sort()
#include <sys/types.h>			// these 3 for stat()
#include <sys/stat.h>
#include <unistd.h>

// temp save 3D png
#include "itkPNGImageIO.h"
#include "itkRescaleIntensityImageFilter.h"

// Setup printing of stack trace on segmentation faults. This only
// works on select GNU systems
#include <signal.h>
#include <execinfo.h>

using namespace std;
using namespace fltk;
using namespace itk;

#define WINXPOS		25
#define WINYPOS		25
#define WINXSIZE	725
#define WINYSIZE	600

#define COLORLABEL_WHITE	6

#define TLIM(x)						// loadImage timing
#define STIM(x)	x					// snake seg timing
#define DMTM(x)	x					// display modTransMatrix in redraw()

Timer	segTime_;

int
main( int argc, char **argv)
{
//	SetupSignalHandlers();

	Preferences app( Preferences::USER, "branham.ws", "Aseg");
	Preferences winPref(app, "win");
	int winflags, winx, winy, winw, winh;
	winPref.get("flags", winflags, 0);
	winPref.get("x", winx, WINXPOS);
	winPref.get("y", winy, WINYPOS);
	winPref.get("w", winw, WINXSIZE);
	winPref.get("h", winh, WINYSIZE);

	Aseg *win = new Aseg( winx, winy, winw, winh, WIN_TITLE);

	run();

	winPref.set("flags", winflags);
	winPref.set("x", win->window()->x());
	winPref.set("y", win->window()->y());
	winPref.set("w", win->window()->w());
	winPref.set("h", win->window()->h());

	delete win;

	return 0;
}

void unpackColor( unsigned int c, int &r, int &g, int &b)
{
	r = c & 0xff;
	g = c>>8 & 0xff;
	b = c>>16 & 0xff;
}

/////////////////////////////////////////////////////////////////////
//
//	Aseg class		Window for 3 image views (Cor, Sag, ???) & 3D model
//
/////////////////////////////////////////////////////////////////////
Aseg::Aseg( int x, int y, int w, int h, const char *title)
	: AsegUI(), _image(0), _regMesh(0), _imageTime(0), _edgeSettingsLoaded(false),
	_thresholdSettingsLoaded(false), _fibrosisHistogram(0),
	_DrawingColorLabel(1), _SegmentationDlog(0), _modelReg(0),
	_ImgWindowUI(0), _MeshWindowUI(0), _AddWallUI(0), _imageInfo(0), _ColorLabelDlog(0),
	_snakeActive(false), _snakeRunning(false), _segUpdateDone(false),
	_showAxial(true), _showSagital(true), _showCoronal(true)

{
	// set Pt3<float> vertex functions
	Pt3<float> pt;
	pt.setVertFcn( glVertex3f);
	pt.setVert2dFcn( glVertex2f);

	// Initialize threshold filter
	_thresholdFilter = new ThresholdFilter();
	_thresholdFilter->setParent( this);

	// Initialize Edge filter UI
	_edgeFilter = new EdgeFilter();
	_edgeFilter->setParent( this);

	// Initialize SetBubbles
	_setBubbles = new SetBubbles( this);

	// Initialize _intensityFilter
	_intensityFilter = new ImageIntensity( this);

	// Initialize main window
	make_window();
	window()->resize( x, y, w, h);
	window()->label( title);
	window()->begin();
//	focus( modelView());		// so _curImgSetText doesn't show text selected
//	window()->resizable( _modelGroup);
	window()->resize_align(ALIGN_TOPLEFT);
	window()->end();
	window()->show();
//	_wizard->show();
//	_wizard->value( _wizGroup0);
//	_wizGroup0->show();
//	_wizGroup0->draw();
	for ( uint i=0; i<3; i++)
	{
		_imgView[i]->setParent( this);
		_imgView[i]->setId( i);
		_imgView[i]->show();
	}
	meshView()->show();
	for ( uint i=0; i<5; i++)
		_MeshDisplayButton[i]->hide();
//	_bigMeshView->show();

	char *strptr = getenv( "IMAGE_DIR");
	if (strptr == 0)
		strptr=getenv("HOME");
	currentDirectory() = strptr;

	_fileChooser = new FileChooser( NULL, NULL, 0, NULL);
	_progress = new ItkProgress( "Loading Images", true, false);

//	_imgView[0]->make_current();	// need GL context for getting font display lists
//	_font = new GlFont( HELVETICA, 12.F);

	_SegParameters = new SegParameters();

	beep_on_dialog(true);	// enable beep on default message dialog (eg. ask, choice, ...)

	_editMenu->child( DELETE_CIRCLE)->deactivate();

	_displayMenu->child( SHOW_SPEED)->deactivate();
	_displayMenu->child( SHOW_SNAKE)->deactivate();
	_showPixelValueToggle->deactivate();
	_showXYToggle->deactivate();

	_displayMenu->child( SHOW_REGMESH)->deactivate();
	_analysisMenu->child( SEGMENT)->deactivate();
	_analysisMenu->child( REG_MODEL)->deactivate();
	_analysisMenu->child( SEG_VOLUME)->deactivate();
	_analysisMenu->child( ALL_SEG_VOLUME)->deactivate();
	_analysisMenu->child( FIBROSIS)->deactivate();
}

Aseg::~Aseg()
{
}

//**********************************************************************
//
//	private functions
//
//**********************************************************************

//**********************************************************************
//
//	virtual menu (UI) functions
//
//**********************************************************************
//
//	File menu items
//
void
Aseg::doStartOps()
{
	StartOps		startOps;
	string soFile = getenv( "HOME");
	soFile += "/AsegStartOps";
	bool ret = true;

	int n = startOps.load( soFile);
	if ( n == 0)
	{
		string msg = "Error loading " + soFile;
		message( msg.c_str());
		return;
	}
	_newStartOps.clear();
	for ( int i = 0; i<n && ret; i++)
	{
		StartOp *so = startOps.get( i);
		switch ( so->key)
		{
			case LoadImages:
				ret = loadImages( so->val.c_str());
				break;
			case LoadRoi:
				ret = loadROI( so->val.c_str());
				break;
			case LoadWalls:
				ret = loadWalls( so->val.c_str());
				break;
			case LoadBubbles:
				ret = loadBubbles( so->val.c_str());
				break;
			case LoadSegmentation:
				ret = loadSegmentation( so->val.c_str());
				break;
			case LoadMesh:
				ret = loadMesh( so->val.c_str());
				break;
		}
	}
	activateMenus();
}

void
Aseg::saveStartOps()
{
	string soFile = getenv( "HOME");
	soFile += "/AsegStartOps";

	_newStartOps.save( soFile);
	message_window_timeout = 1;
	soFile += " - saved";
	message( soFile.c_str());
	message_window_timeout = 0;
}

bool
Aseg::loadImages( const char *imgFileName)
{
	ItkImageIO3D io;
	bool imagesLoaded;
TLIM(Timer tim;)

	if ( _mainImage.size())
	{
		if ( ask( "Images already loaded, delete & continue?"))
		{
			// delete current images
			for ( uint i=0; i < _mainImage.size(); i++)
			{
				if ( _mainImage[i]->NativeImageIsLoaded())
					_mainImage[i]->DeallocateNativeImage();
				delete _mainImage[i];
				delete _segImage3D[i];
			}
			_mainImage.clear();
			_segImage3D.clear();
			vector<float> spc;
			setImage( 0, spc);					// make sure ImgViews have null image
			setDisplayType( DT_MAIN);

			// clear meshes
			mesh()->Reset();
			segMesh()->Reset();
			if ( regMesh())
			{
				delete regMesh();
				_regMesh = 0;
			}
			_showRegMeshToggle->value( false);
			_showRegMeshToggle->deactivate();
			_showMeshToggle->value( false);
			_showSegmentationToggle->value( false);
			_newStartOps.clear();
		}
		else
			return false;
	}
	window()->make_current();
	if ( imgFileName == 0)
	{
		_fileChooser->directory( currentDirectory().c_str());
		_fileChooser->type( FileChooser::MULTI | FileChooser::DIRECTORY);
		_fileChooser->label("Load Image");
		_fileChooser->filter( "*.lst");

		_fileChooser->show( 300, 300);

		while ( _fileChooser->visible())
			fltk::wait();

		if ( _fileChooser->count() > 0)
			imgFileName = _fileChooser->value();
	}

	if ( imgFileName)
	{
		setCursor( XC_watch);
		centerChildWindow( progress()->window());
		progress()->range( 0.0, 100.0);
		progress()->show();
		progress()->position( 0.0);
		progress()->redraw();	check();
		setStatus( "Loading images");
		char *fname = strdup( imgFileName);
		char *fn1;
		if ( !isDirectory( imgFileName) && (strstr( imgFileName, ".lst") == 0))
		{	// go up two levels if not a directory and not a .lst
			fn1 = dirName( fname);
			fn1 = dirName( fn1);
		}
		else
			fn1 = dirName( fname);
		currentDirectory() = fn1;
		chdir( currentDirectory().c_str());		// so relative pathnames ok if .lst
		try
		{
			io.Read( imgFileName, progress()->GetRedrawCommand());
			if ( !io.ImagesLoaded())
			{	// problem loading or canceled
				setCursor( XC_left_ptr);			// get rid of wait cursor
				progress()->hide();
				return false;
			}
			for ( uint i=0; i < io.NumImages(); i++)
			{
				_mainImage.push_back( new ItkImage3D);
				_mainImage[i]->SetNativeImage( io.GetNativeImage(i));
				_mainImage[i]->printInfo();
				_segImage3D.push_back( new SegImage3D);
				_segImage3D[i]->SetNativeImage( io.GetNativeImage(i));
			}

			imagesLoaded = true;
		}
		catch (itk::ExceptionObject &exc)
		{
			// Show the error
			alert("Error reading image: %s.", exc.GetDescription());
			progress()->hide();
			imagesLoaded = false;
		}
	}
	else
		return false;

	if ( imagesLoaded)
	{
		setStatus( "Initializing images");
		progress()->label( "Initializing images");
		progress()->position( 0.0);
		progress()->redraw();	check();
		_imageTime = 0;
		fileName() = imgFileName;

		// initialize image transforms
		// See Logic/Framework/IRISApplication::SetDisplayToAnatomyRAI(),
		// Logic/Framework/GenericImageData::SetImageGeometry(),
		// Logic/Common/ImageCoordinateGeometry::SetGeometry()
		string DisplayToAnatomyRAI[3] = {"RPS", "AIL", "RIP"};
		_mainROI = mainImage()->GetNativeImage()->GetBufferedRegion();
		_segROI = _mainROI;		// compiler didn't like _mainROI = _segROI = image...

		Vector3ui vsize = to_unsigned_int(Vector3ul( mainROI().GetSize().GetSize()));

		_ImageCoordinateGeometry.SetGeometry(
			mainImage()->GetNativeImage()->GetDirection().GetVnlMatrix(),
			DisplayToAnatomyRAI, vsize);

		// from UserInterface/MainComponents/UserInterfaceLogic.cxx::OnMenuLoadGrey() ->
		// from Logic/Framework/IRISApplication::UpdateIRISMainImage.cxx
		// Rescale the image to grey
TLIM(	cout << "SetGrey: " << tim.msec() << endl;)
		for ( uint i=0; i < io.NumImages(); i++)
		{
			RescaleNativeImageToScalar<GreyType> rescaler;		// class in ItkImageIO3D
			ItkImage3D::GreyImageType::Pointer imgGrey = rescaler( &io, i);
			GreyTypeToNativeFunctor mapper( rescaler.GetNativeScale(),
				rescaler.GetNativeShift());
			_mainImage[i]->SetGreyImage( imgGrey, _ImageCoordinateGeometry, mapper);
			_mainImage[i]->setFileNames( io.fileNames());
			_segImage3D[i]->SetGreyImage( imgGrey, _ImageCoordinateGeometry, mapper);
			_segImage3D[i]->SetColorLabelTable( GetColorLabelTable());
			progress()->position( 30.0 + 40.0 * (double(i) / double(io.NumImages())));
			progress()->redraw();	check();
		}
TLIM(	cout << "initSeg: " << tim.msec() << endl;)
		cout << "loadImages: NiftiSform:" << endl << mainImage()->GetGrey()->GetNiftiSform()
			<< endl;

		initSegImage();		//	Initialize the segImage for segmentation

		// initialize ImgView's & slice scrollbars
		for ( uint i=0; i<3; i++)
			_imageAxes[i] = mainImage()->GetGrey()->GetDisplaySliceImageAxis( i);

TLIM(	cout << "setImage: " << tim.msec() << endl;)
		progress()->position( 70.0);
		progress()->redraw();	check();
		_image = mainImage();		// need before setImage() for MeshView::initialize()
		setImage( mainImage()->GetGrey(), mainImage()->PixelSpacing());	// imgViews, MeshView
		setDisplayType( DT_MAIN);

		// set threshold & edge settings defaults
TLIM(	cout << "settings: " << tim.msec() << endl;)
		getThresholdSettings().MakeDefaultSettings( image()->GetGrey());
		getEdgeSettings().MakeDefaultSettings();

		// initialize window sliders
		initSliders();
		_timeSlider->range( 1.0, io.NumImages());
		_timeSlider->value( 1.0);

		// rotate cube a little
//		meshView()->rotate( float(20), float(20));
		mesh()->Initialize( this);			// just sets parent pointer
		segMesh()->Initialize( this);

		// At this point, deallocate the native image, so that we don't use more memory
		io.DeallocateNativeImage();

		progress()->hide();

		// add to StartOps
		_newStartOps.add( LoadImages, imgFileName);

		// display images
		sliceChange();		// does redraw();
		activateMenus();
		string msg = "Loaded ";
		msg += imgFileName;
		setStatus( msg.c_str());


//		Save 3D .png of image
		typedef itk::ImageFileWriter< LabelImageType>   WriterType;
		typename WriterType::Pointer writer = WriterType::New();
		writer->SetImageIO( itk::PNGImageIO::New());
		writer->SetFileName( "img3D.png");
		// need unsigned short, have short
		typedef itk::RescaleIntensityImageFilter<ItkImage3D::GreyImageType, LabelImageType> RescaleFilterType;
		typename RescaleFilterType::Pointer rescaleFilter = RescaleFilterType::New();
		rescaleFilter->SetInput( mainImage()->GetGrey()->GetImage());
		rescaleFilter->SetOutputMinimum( 0);
		rescaleFilter->SetOutputMaximum( 32767);

		writer->SetInput( rescaleFilter->GetOutput());
		writer->Update();

		setCursor( XC_left_ptr);			// get rid of wait cursor
TLIM(	cout << "done: " << tim.msec() << endl;)
		return true;
	}
	setCursor( XC_left_ptr);			// get rid of wait cursor
	progress()->hide();

	return false;
}	// loadImages()

bool
Aseg::loadROI( const char *fileName)
{
	ifstream roiFile;

	if ( fileName == 0)
	{
		_fileChooser->directory( currentDirectory().c_str());
		_fileChooser->type( FileChooser::SINGLE);
		_fileChooser->label("Load ROI");
		_fileChooser->filter( "*.roi");

		_fileChooser->show( 300, 300);

		while ( _fileChooser->visible())
			fltk::wait();

		if ( _fileChooser->count() > 0)
			fileName = _fileChooser->value();
	}
	roiFile.open( fileName, ios::in);
	if ( roiFile)
	{
		char line[256];
		int dim;
		itk::Index<3u> in;
		itk::Size<3u> sz;
		int i1, i2, i3;
		setCursor( XC_watch);

		roiFile.getline( line, 256);
		if ( !strstr( line, "ImageRegion"))
		{
			alert( "Error, %s is not an ROI file", fileName);
			roiFile.close();
			setCursor( XC_left_ptr);			// get rid of wait cursor
			return false;
		}
		roiFile.getline( line, 256);
		sscanf( line, "	 Dimension: %d", &dim);
		if ( dim != 3)
		{
			alert( "Error %s has dimension %d, not 3", fileName, dim);
			roiFile.close();
			setCursor( XC_left_ptr);			// get rid of wait cursor
			return false;
		}
		roiFile.getline( line, 256);
		sscanf( line, "  Index: [%d, %d, %d]", &i1, &i2, &i3);	// work around bug scanf to
		in[0] = i1, in[1] = i2, in[2] = i3;						// &in[0], &in[2]
		segROI().SetIndex( in);
		roiFile.getline( line, 256);
		sscanf( line, "  Size: [%d, %d, %d]", &sz[0], &sz[1], &sz[2]);
		segROI().SetSize( sz);
		roiFile.close();

		// adjust SegImage3D walls for new ROI
		sz = mainROI().GetSize();
		itk::Size<3u> rsz = segROI().GetSize();
		Pt3<float> indxPt( -(float)in[0], -(float)(sz[1] - rsz[1] - in[1]), float(0));
		segImage3D()->adjustWallIndex( indxPt);

		initROI();
		redraw();

		setCursor( XC_left_ptr);			// get rid of wait cursor

		// update ImgView crosshairs
		Pt3<float> cur( float(_cursor.x()), float(_cursor.y()), float(_cursor.z()));
		setCrosshairs( cur);

		// add to StartOps
		_newStartOps.add( LoadRoi, fileName);

		setStatus( "ROI loaded");
		activateMenus();
		return true;
	}
	return false;
}	// loadROI()

void
Aseg::saveROI()
{
	_fileChooser->directory( currentDirectory().c_str());
	_fileChooser->type( FileChooser::CREATE);
	_fileChooser->label("Save ROI");
	_fileChooser->filter( "*.roi");

	_fileChooser->show( 300, 300);

	while ( _fileChooser->visible())
		fltk::wait();

	if ( _fileChooser->count() > 0)
	{
		ofstream roiFile;
		string fileName = _fileChooser->value();
		if ( fileName.find( ".roi") == string::npos)
			fileName += ".roi";
		roiFile.open( fileName.c_str(), ios::out);
		segROI().Print( roiFile);
		roiFile.close();
	}
}

bool
Aseg::loadBubbles( const char *fileName)
{
	ifstream inFile;

	if ( fileName == 0)
	{
		_fileChooser->directory( currentDirectory().c_str());
		_fileChooser->type( FileChooser::SINGLE);
		_fileChooser->label("Load Bubbles");
		_fileChooser->filter( "*.bub");

		_fileChooser->show( 300, 300);

		while ( _fileChooser->visible())
			fltk::wait();

		if ( _fileChooser->count() > 0)
			fileName = _fileChooser->value();
	}
	inFile.open( fileName, ios::in);
	if ( inFile)
	{
		uint nBubbles;
		string str;
		Sphere<uint> sphere;

		bubbles().clear();
		inFile >> str;
		if ( str == "Bubbles")
		{
			inFile >> nBubbles;
			for ( uint i=0; i < nBubbles; i++)
			{
				inFile >> sphere;
				bubbles().push_back( sphere);
			}
		}
		else
			alert( "%s is not a bubbles file", fileName);
		inFile.close();

		if ( bubbles().size())
		{
			setBubblesOk();

			// add to StartOps
			_newStartOps.add( LoadBubbles, fileName);

			string msg = "loaded bubbles: ";
			msg += fileName;
			setStatus( msg.c_str());

			setViewMode( ImgView::VM_BUBBLE);
			redraw();
			activateMenus();
			return true;
		}
	}
	else
		alert( "Error opening %s", fileName);

	return false;
}	// loadBubbles()

void
Aseg::saveBubbles()
{
	_fileChooser->directory( currentDirectory().c_str());
	_fileChooser->type( FileChooser::CREATE);
	_fileChooser->label("Save Bubbles");
	_fileChooser->filter( "*.bub");

	_fileChooser->show( 300, 300);

	while ( _fileChooser->visible())
		fltk::wait();

	if ( _fileChooser->count() > 0)
	{
		ofstream outFile;
		string fileName = _fileChooser->value();
		if ( fileName.find( ".bub") == string::npos)
			fileName += ".bub";
		outFile.open( fileName.c_str(), ios::out);
		if ( outFile)
		{
			outFile << "Bubbles" << endl;
			outFile << bubbles().size() << endl;
			for ( vector<Sphere<uint> >::iterator bi = bubbles().begin();
					bi != bubbles().end(); bi++)
				outFile << *bi << endl;
			outFile.close();
		}
	}
}

//
//	Miss named, load image to be used to init segmentation
//
bool
Aseg::loadSegmentation( const char *fileName)
{
	ItkImageIO3D io;

	if ( fileName == 0)
	{
		_fileChooser->directory( currentDirectory().c_str());
		_fileChooser->type( FileChooser::SINGLE);
		_fileChooser->label("Load Segmentation");
		_fileChooser->filter( "*.gipl");

		_fileChooser->show( 300, 300);

		while ( _fileChooser->visible())
			fltk::wait();

		if ( _fileChooser->count() > 0)
		{
			fileName = _fileChooser->value();
		}
	}
	if ( fileName)
	{
		try
		{
			io.Read( fileName);
			// Cast the image to label type
			CastNativeImageToScalar<LabelTypeUS> caster;	// Cast class in ItkImageIO3D
			_segImage = _segROIImage = caster( &io);

			// The header of the label image is made to match that of the grey image
			_segImage->SetOrigin( mainImage()->GetGrey()->GetImageBase()->GetOrigin());
			_segImage->SetSpacing( mainImage()->GetGrey()->GetImageBase()->GetSpacing());
			_segImage->SetDirection( mainImage()->GetGrey()->GetImageBase()->GetDirection());

			// Also set _segImage ROI if any
			if ( mainROI() != segROI())
			{
				RoiFilterType::Pointer ROIfilter = RoiFilterType::New();
				ROIfilter->SetInput( _segImage);
				ROIfilter->SetRegionOfInterest( segROI());
				ROIfilter->Update();
				_segROIImage = ROIfilter->GetOutput();
			}

			// set initial segmentation image
			if ( segImage3D()->SetSegmentationImage( _segROIImage))
			{
				meshOptions().SetUseDecimation( true);
				meshOptions().SetDecimateTargetReduction( 0.99F);
				updateMesh( true);
			}
			else
			{
				alert( "Error initializing segmentation");
				return false;
			}

			// print Nifti xform
			cout << "loadSegmentation: NiftiSform:" << endl
				<< segImage3D()->GetSegmentation()->GetNiftiSform() << endl;

			segImage3D()->SetColorLabelTable( GetColorLabelTable());

			initSegImage();		//	Initialize the segImage for segmentation

			_SegParameters->setDirectory( _currentDirectory);
		}
		catch (itk::ExceptionObject &exc)
		{
			// Show the error
			alert("Error reading gipl image: %s.",exc.GetDescription());
			return false;
		}

		// add to StartOps
		_newStartOps.add( LoadSegmentation, fileName);

		if ( regMesh())
		{
			delete regMesh();
			_regMesh = 0;
		}
		segMesh()->Reset();
		_showRegMeshToggle->value( false);
		_showRegMeshToggle->deactivate();
		if ( mesh()->GetNumberOfVTKMeshes() == 0)
			_showMeshToggle->value( false);

		// set up ImgViews
		setLabelImage( segImage3D()->GetSegmentation());		// m_LabelWrapper->ImgViews
		setDisplayType( DT_LABLE);

		string msg = "loaded segmentation: ";
		msg += fileName;
		setStatus( msg.c_str());
		activateMenus();
		redraw();
	}
	return true;
}	// loadSegmentation()

void
Aseg::saveSegmentation()
{
	ItkImageIO3D io;

	_fileChooser->directory( currentDirectory().c_str());
	_fileChooser->type( FileChooser::CREATE);
	_fileChooser->label("Save Segmentation");
	_fileChooser->filter( "*.gipl");

	_fileChooser->show( 300, 300);

	while ( _fileChooser->visible())
		fltk::wait();

	if ( _fileChooser->count() > 0)
	{
		string fileName = _fileChooser->value();
		if ( fileName.find( ".gipl") == string::npos)
			fileName += ".gipl";
		try
		{
			io.SaveGiplImage( fileName.c_str(),
								segImage3D()->GetSegmentation()->GetImage());
		}
		catch (itk::ExceptionObject &exc)
		{
			// Show the error
			alert("Error writing .gipl image: %s.",exc.GetDescription());
		}
	}
}

//
//	Load mesh to be used to init segmentation
//
bool
Aseg::loadMesh( const char *fileName)
{
	if ( fileName == 0)
	{
		_fileChooser->directory( currentDirectory().c_str());
		_fileChooser->type( FileChooser::SINGLE);
		_fileChooser->label("Load Mesh");
		_fileChooser->filter( "*.vtm");

		_fileChooser->show( 300, 300);

		while ( _fileChooser->visible())
			fltk::wait();

		if ( _fileChooser->count() > 0)
		{
			fileName = _fileChooser->value();
		}
	}
	if ( fileName)
	{
		if ( mesh()->loadMesh( fileName))
		{
			// add to StartOps
			_newStartOps.add( LoadMesh, fileName);

			string msg = "loaded mesh: ";
			msg += fileName;
			setStatus( msg.c_str());

			// ImgView's,  MeshView mesh() set in loadImages() -> meshView()->initialize()
			meshView()->resetRegistration();		// set registration matrix's to identity
			if ( _MeshWindowUI)
			{
				_MeshWindowUI->meshView()->resetRegistration();
				_MeshWindowUI->meshView()->initialize( this, mesh());
			}
			if ( regMesh())
			{
				delete regMesh();
				_regMesh = 0;
			}
			segMesh()->Reset();
			segImage3D()->clearSegmentation();
			setImgViewMesh();
			updateMeshToggles();

			activateMenus();
			_showMeshToggle->value( true);
			showMesh();				// calls redraw()
			return true;
		}
		else
		{
			alert( "Error loading %s", fileName);
			return false;
		}
	}
}	// loadMesh()

bool
Aseg::saveMesh( uint type)
{
	_fileChooser->directory( currentDirectory().c_str());
	_fileChooser->type( FileChooser::CREATE);
	_fileChooser->label("Save Mesh");
	_fileChooser->filter( "*.vtm");

	_fileChooser->show( 300, 300);

	while ( _fileChooser->visible())
		fltk::wait();

	if ( _fileChooser->count() > 0)
	{
		bool res;
		string fileName = _fileChooser->value();
		if ( fileName.find( ".vtm") == string::npos)
			fileName += ".vtm";
		if ( type == 1)
			res = regMesh()->saveMesh( fileName.c_str());
		else
			res = segMesh()->saveMesh( fileName.c_str());
		if ( !res)
		{
			alert( "Error saving %s", fileName.c_str());
			return false;
		}
	}
	return true;
}

//
//	Load walls
//
bool
Aseg::loadWalls( const char *fileName)
{
	ifstream inFile;

	if ( fileName == 0)
	{
		_fileChooser->directory( currentDirectory().c_str());
		_fileChooser->type( FileChooser::SINGLE);
		_fileChooser->label("Load Walls");
		_fileChooser->filter( "*.con");

		_fileChooser->show( 300, 300);

		while ( _fileChooser->visible())
			fltk::wait();

		if ( _fileChooser->count() > 0)
			fileName = _fileChooser->value();
	}
	inFile.open( fileName, ios::in);
	if ( inFile)
	{
		itk::Index<3u> in = segROI().GetIndex();
		itk::Size<3u> sz = mainROI().GetSize();
		itk::Size<3u> rsz = segROI().GetSize();
		Pt3<float> indxPt( -(float)in[0], -(float)(sz[1]-rsz[1]-in[1]), float(0));

		segImage3D()->loadWalls( inFile, indxPt);
//		generateWallMeshes();

		// add to StartOps
		_newStartOps.add( LoadWalls, fileName);

		string msg = "loaded walls: ";
		msg += fileName;
		setStatus( msg.c_str());

		redraw();
		activateMenus();
		return true;
	}
	else
		alert( "Error loading %s", _fileChooser->value());
	return false;
}

void
Aseg::saveWalls()
{
	_fileChooser->directory( currentDirectory().c_str());
	_fileChooser->type( FileChooser::CREATE);
	_fileChooser->label("Save Walls");
	_fileChooser->filter( "*.con");

	_fileChooser->show( 300, 300);

	while ( _fileChooser->visible())
		fltk::wait();

	if ( _fileChooser->count() > 0)
	{
		ofstream outFile;
		string fileName = _fileChooser->value();
		if ( fileName.find( ".con") == string::npos)
			fileName += ".con";
		outFile.open( fileName.c_str(), ios::out);
		if ( outFile)
		{
			itk::Index<3u> in = segROI().GetIndex();
			itk::Size<3u> sz = mainROI().GetSize();
			itk::Size<3u> rsz = segROI().GetSize();
			Pt3<float> indxPt( (float)in[0], (float)(sz[1]-rsz[1]-in[1]), float(0));

			segImage3D()->saveWalls( outFile, indxPt);
		}
	}
}

void
Aseg::saveFibrosis()
{
	if ( _fibrosisHistogram == 0)
		return;

	_fileChooser->directory( currentDirectory().c_str());
	_fileChooser->type( FileChooser::CREATE);
	_fileChooser->label("Save Fibrosis");
	_fileChooser->filter( "*.fib");

	_fileChooser->show( 300, 300);

	while ( _fileChooser->visible())
		fltk::wait();

	if ( _fileChooser->count() > 0)
	{
		ofstream outFile;
		string fileName = _fileChooser->value();
		if ( fileName.find( ".fib") == string::npos)
			fileName += ".fib";
		outFile.open( fileName.c_str(), ios::out);
		if ( outFile)
		{
			segImage3D()->saveFibrosis( outFile, _fibrosisHistogram->minPixel(),
				_fibrosisHistogram->maxPixel(), _fibrosisHistogram->getThreshold(),
				_fibrosisHistogram->getAboveThresh(),
				_fibrosisHistogram->getPerCentAboveThresh());

			message_window_timeout = 2;
			alert( "%s saved", baseName((char *)fileName.c_str()));
			message_window_timeout = 0;
		}
	}
	else
		alert( "Error opening %s", _fileChooser->value());
}

void
Aseg::quit()
{
//	delete _font;
	for ( uint i=0; i<3; i++)
		_imgView[i]->hide();
	meshView()->hide();

	_thresholdFilter->hide();
	delete _thresholdFilter;

	_edgeFilter->hide();
	delete _edgeFilter;

	bubblesUI()->hide();
	delete bubblesUI();

	_intensityFilter->hide();
	delete _intensityFilter;

	if ( _SegParameters)
	{
		_SegParameters->hide();
		delete _SegParameters;
	}
	if ( segmentationDlog())
	{
		segmentationDlog()->hide();
		delete segmentationDlog();
	}
	if ( _ImgWindowUI)
	{
		_ImgWindowUI->_imgWindow->hide();
		delete _ImgWindowUI;
	}
	if ( _MeshWindowUI)
	{
		_MeshWindowUI->_meshWindow->hide();
		delete _MeshWindowUI;
	}
	if ( _AddWallUI)
	{
		_AddWallUI->hide();
		delete _AddWallUI;
	}
	if ( _imageInfo)
	{
		_imageInfo->hide();
		delete _imageInfo;
	}
	delete _fileChooser;
	progress()->hide();
	delete progress();

	window()->hide();

	for ( uint i=0; i < _mainImage.size(); i++)
	{
		delete _mainImage[i];
		delete _segImage3D[i];
	}
	if ( _fibrosisHistogram)
	{
		_fibrosisHistogram->hide();
		delete _fibrosisHistogram;
	}
}

void
Aseg::editSegmentationParameters()
{
	_SegParameters->setDirectory( _currentDirectory);
	_SegParameters->post( 0, editParametersOkCB, editParametersCancel, 0, this);
	_SegParameters->setGeodMean( "PCAMean.mha");
	_SegParameters->setGeodModeFmt( "PCAMode%d.mha");
}

void
Aseg::setEdgeSettings()
{
	_displayMenu->child( SHOW_SPEED)->activate();
	_edgeFilter->post( 0, EdgeFilterOkCB, EdgeFilterCancelCB, EdgeFilterApplyCB, this);
	_edgeFilter->initialize();		// calls EdgePreviewChange()
	_edgeFilter->setDirectory( _currentDirectory);
	setSpeedImage( 0);								// ImgViews clear any 3D speed image
	setInitShapeImg( 0);
	setSpeedImage2D( segImage3D()->GetSpeed2D());	// ImgViews
	if ( _edgeFilter->showPreview())
	{
		setDisplayType( DT_SPEED);
	}
	redraw();
}

void
Aseg::setThresholdSettings()
{
	_displayMenu->child( SHOW_SPEED)->activate();
	_thresholdFilter->post( 0, ThresholdFilterOkCB, ThresholdFilterCancelCB,
		ThresholdFilterApplyCB, this);
	_thresholdFilter->initialize();
	_thresholdFilter->setDirectory( _currentDirectory);
	setSpeedImage( 0);
	setInitShapeImg( 0);							// ImgViews clear any init speed image
	setSpeedImage2D( segImage3D()->GetSpeed2D());		// ImgViews
	if ( _thresholdFilter->showPreview())
	{
		setDisplayType( DT_SPEED);

	}
	redraw();
}

void
Aseg::setBubbles()
{
	bubblesUI()->post( 0, setBubblesOkCB, setBubblesCancelCB, 0, this);
	if ( bubbles().size())
		bubblesUI()->updateBrowser();
	setViewMode( ImgView::VM_BUBBLE);
	redraw();
}

//
//	Reset Segmentation.	Called from doSegmentation().
//
bool Aseg::resetSegmentation()
{
	stopSegmentation();						// stops thread, updates mesh

	setStatus( "Initializing segmentation");

	// In case loaded already registered mesh.
	if ( regMesh() == 0 && mesh()->GetNumberOfVTKMeshes())
	{
		_regMesh = new MeshObject( *mesh());
		regMesh()->GenerateDisplayLists();
	}

	if ( segImage3D()->LevelSetDriver())
		segImage3D()->TerminateSegmentation();

	// reset SegImage3D label image to _segImage if have or clear so use bubbles only
	if ( regMesh())
		_segImage = _segROIImage =  regMesh()->GetLabelImage();

	if ( _segImage)
		segImage3D()->SetSegmentationImage( _segImage);
	else
		segImage3D()->clearSegmentation();

	// Merge bubbles with the segmentation image and initialize the snake. Starts segmentation.
	if ( !segImage3D()->InitializeSegmentation( _SegParameters, bubbles(),
			GetDrawingColorLabel()))	// no progress supplied
	{
		alert( "Error initializing segmentation");
		return false;
	}

	// display initial shape if doing GeoActContShape seg
	if ( _SegParameters->GetSolver() == SegParameters::GEO_ACT_CONT_SHAPE &&
		segImage3D()->GetShape())
	{
		setInitShapeImg( segImage3D()->GetShape());
		showInitialShape();
	}

	snakeUpdate();				// updates mesh
	segMesh()->Reset();
	segmentationDlog()->setIterations( 0);
	segmentationDlog()->setSeconds( 0);

	redraw();
	return true;
}

//
//	Clear Segmentation.	Called from UI/Edit/'Clear Segmentation'.
//
void Aseg::clearSegmentation()
{
	segImage3D()->clearSegmentation();
	segMesh()->Reset();
	initRegMesh();
	setImgViewMesh();					// clears ImgView's segMesh because was reset
	_editMenu->child( CLEAR_SEGMENTATION)->deactivate();
	_showSegmentationToggle->value( false);
	redraw();
}

void
Aseg::setColor()
{
	if ( _ColorLabelDlog == 0)
	{
		_ColorLabelDlog = new ColorLabelDlog();
	}
	_ColorLabelDlog->post( 0, colorOkCB, 0,	0, this);
	for ( uint c = 0; c < 5; c++)
	{
		const ColorLabel &cl = GetColorLabelTable()->GetColorLabel(c+1);
		unsigned char array[3];
		cl.GetRGBVector( array);
		uint ci = array[0] << 24 | array[1] << 16 | array[2] << 8;
		_ColorLabelDlog->setLabelColor( c, ci);
	}
}

void Aseg::addWall( uint wt)
{
	segImage3D()->addWall( wt);
	setViewMode( ImgView::VM_DRAW_WALL);
	setStatus( "Draw Wall");
	if ( !_AddWallUI)
		_AddWallUI = new AddWallUI();
	_AddWallUI->post( 0, AddWallOkCB, AddWallCancelCB, 0, this);
}

void Aseg::editWall()
{
	if ( _editWallToggle->value())
	{
		segImage3D()->editWall( true);
		setViewMode( ImgView::VM_EDIT_WALL);
	}
	else
	{
		segImage3D()->editWall( false);
		segImage3D()->curWall(SegImage3D::NOWALL);		// no wall points
		setViewMode( ImgView::VM_CROSS_HAIR);
		_editMenu->child( DELETE_WALL)->deactivate();
//		generateWallMeshes();
		redraw();
	}
}

void Aseg::deleteWall()
{
	if ( segImage3D()->wallSelected())
	{
		segImage3D()->deleteWall();
		segImage3D()->curWall(SegImage3D::NOWALL);		// no wall points
		segImage3D()->editWall( false);
		_editWallToggle->value( false);
		_editMenu->child( DELETE_WALL)->deactivate();
	//	generateWallMeshes();
		activateMenus();
		redraw();
	}
}

void
Aseg::copyWalls( uint n)
{
	segImage3D()->copyWalls( n);
	redraw();
}

void
Aseg::drawCircle()
{
	imgView(0)->setMode( ImgView::VM_DRAW_CIRCLE);
	setStatus( "Draw circle");
	_editMenu->child( DELETE_CIRCLE)->activate();
}

void
Aseg::deleteCircle()
{
	imgView(0)->clearPoly();
	imgView(0)->setMode( ImgView::VM_CROSS_HAIR);
	setStatus( "");
	_editMenu->child( DELETE_CIRCLE)->deactivate();
}

void
Aseg::setIntensity()
{
	_intensityFilter->post(0, intensityOkCB, intensityCancelCB, this);
	_intensityFilter->setValues( segImage3D()->GetGrey()->GetImageMin(),
								segImage3D()->GetGrey()->GetImageMax());
}

//-----------------------------------------------------------------------------
//
//	Display menu items
//
//-----------------------------------------------------------------------------

//
//	What about MeshObjects _mesh, _segMesh???
//
void
Aseg::setROI()
{
	ImgView::ViewMode mode;

	if ( _setRoiButton->value())
	{
		mode = ImgView::VM_ZOOM;
	}
	else
	{
		if ( roiChanged())
		{
			initROI();

			// adjust SegImage3D walls for new ROI
			itk::Index<3u> in = segROI().GetIndex();
			itk::Size<3u> sz = mainROI().GetSize();
			itk::Size<3u> rsz = segROI().GetSize();
			Pt3<float> indxPt( -(float)in[0], -(float)(sz[1]-rsz[1]-in[1]), float(0));
			segImage3D()->adjustWallIndex( indxPt);
		}
		mode = ImgView::VM_CROSS_HAIR;
	}

	setViewMode( mode);
	redraw();

	activateMenus();
}

//
//	Reset ROI to mainImage original ROI
//
void
Aseg::resetROI()
{
	// adjust SegImage3D walls for no ROI
	itk::Index<3u> in = segROI().GetIndex();
	itk::Size<3u> sz = mainROI().GetSize();
	itk::Size<3u> rsz = segROI().GetSize();
	Pt3<float> indxPt( (float)in[0], (float)(sz[1]-rsz[1]-in[1]), float(0));
	segImage3D()->adjustWallIndex( indxPt);

	_segROI = _mainROI;

	segImage3D()->SetGreyImage( mainImage()->GetGrey()->GetImage(), _ImageCoordinateGeometry,
								mainImage()->GetGrey()->GetNativeMapping());
	if ( segImage())
		segImage3D()->SetSegmentationImage( segImage());

	// clear any existing ImgView images
//	vector<float> spacing;
	setInitShapeImg( 0);
	setSpeedImage( 0);
	setSpeedImage2D( 0);
	setSnakeImage( 0);
//?	setLabelImage( 0);
	_thresholdSettingsLoaded = false;

	initSegImage();		//	Initialize the segImage for segmentation

	setImage( mainImage()->GetGrey(), mainImage()->PixelSpacing());
	_image = mainImage();

	setLabelImage( segImage3D()->GetSegmentation());			// could be zero

//	bubbles().clear();

	for ( uint i=0; i<3; i++)
	{
		imgView(i)->resetRoi();
	}
	setDisplayType( DT_MAIN);
	initSliders();

//	segImage3D()->DeallocateNativeImage();			// why?
	_setRoiButton->activate();
	_resetRoiItem->deactivate();

	sliceChange();									// calls redraw()
	activateMenus();
}

// Display menu - 'Register'
void
Aseg::registerMesh()
{
	if ( registrationOn())
	{	// display mesh() during registration
		if ( regMesh() == 0)
		{
			_regMesh = new MeshObject( *mesh());
			regMesh()->GenerateDisplayLists();
			activateMenus();		// turn on 'Save Init Mesh'
		}
		_showMeshToggle->value( false);
		_showRegMeshToggle->activate();
		_showRegMeshToggle->value( true);
		meshView()->setViewMode( MeshView::VM_REGISTER);
		if ( _MeshWindowUI)
			_MeshWindowUI->meshView()->setViewMode( MeshView::VM_REGISTER);
		setImgViewMesh();
		meshView()->regMesh( regMesh());
		if ( _MeshWindowUI)
			_MeshWindowUI->meshView()->regMesh( regMesh());
		setStatus( "Register mesh.  When done be sure to click 'Register Mesh' off.");
	}
	else
	{
		setCursor( XC_watch);
		setStatus( "Wait for getting new registration image.");	flush();
		meshView()->setViewMode( MeshView::VM_NONE);
		if ( _MeshWindowUI)
			_MeshWindowUI->meshView()->setViewMode( MeshView::VM_NONE);
		_segImage = _segROIImage = regMesh()->GetLabelImage();
		segImage3D()->SetSegmentationImage( _segImage);
		setLabelImage( segImage3D()->GetSegmentation());		// m_LabelWrapper->ImgViews
		setCursor( XC_left_ptr);			// get rid of wait cursor
		setStatus( "Registration done.");
	}
	activateMenus();
	redraw();
}

// Display menu - 'Reset Registration'
void
Aseg::resetRegistration()
{
	meshView()->resetRegistration();
	if ( _MeshWindowUI)
		_MeshWindowUI->meshView()->resetRegistration();
	if ( regMesh())
		delete regMesh();
	_regMesh = new MeshObject( *mesh());
	regMesh()->GenerateDisplayLists();
	meshView()->regMesh( regMesh());
	if ( _MeshWindowUI)
		_MeshWindowUI->meshView()->regMesh( regMesh());
	setImgViewMesh();
	redraw();
}

// Display menu - 'Show Bubbles'
void
Aseg::showBubbles()
{
	if ( showBubblesToggle() && haveBubbles())
	{
		setDisplayType( DT_BUBBLES);
	}
	else
	{
		setDisplayType( DT_MAIN);
	}
	redraw();
}

// Display menu - 'Show Mesh'
void
Aseg::showMesh()
{
	if ( showMeshToggle())
	{
		_showRegMeshToggle->value( false);		// radio
		for ( uint wt=0; wt < 5; wt++)
		{
			bool show = mesh()->GetVTKMesh( wt) != 0;

			_MeshDisplayButton[wt]->value( show);
			meshView()->showMesh( wt, show);	// sets MeshView::_showMesh boolean
			if ( _MeshWindowUI)
			{
				_MeshWindowUI->_MeshDisplayButton[wt]->value( show);
				_MeshWindowUI->meshView()->showMesh( wt, show);
			}
		}
	}
	redraw();
}

// Display menu - 'Show Registered Mesh'
void
Aseg::showRegisteredMesh()
{
	if ( regMesh() == 0)
	{
		cerr << "no regMesh" << endl;
		return;
	}
	if ( showRegMeshToggle())
	{
		_showMeshToggle->value( false);		// radio
		for ( uint wt=0; wt < 5; wt++)
		{
			bool show = regMesh()->GetVTKMesh( wt) != 0;

			_MeshDisplayButton[wt]->value( show);
			meshView()->showMesh( wt, show);	// sets MeshView::_showMesh boolean
			if ( _MeshWindowUI)
			{
				_MeshWindowUI->_MeshDisplayButton[wt]->value( show);
				_MeshWindowUI->meshView()->showMesh( wt, show);
			}
		}
	}
	redraw();
}

// Display menu - 'Show Speed'
void
Aseg::showSpeed()
{
	if ( _showSpeedToggle->value() && (haveThresholdSettings() || haveEdgeSettings()))
	{
		setDisplayType( DT_SPEED);
	}
	else
	{
		setDisplayType( DT_MAIN);
	}
	redraw();
}

// Display menu - 'Show initial shape'
void
Aseg::showInitialShape()
{
	if ( _showInitShapeToggle->value() && (haveThresholdSettings() || haveEdgeSettings()))
	{
		setDisplayType( DT_INIT);
	}
	else
	{
		setDisplayType( DT_MAIN);
	}
	redraw();
}

// Display menu - 'Show Snake'
void
Aseg::showSnake()
{
	if ( showSnakeToggle() && segImage3D()->IsSnakeLoaded())
	{
		setSnakeImage( segImage3D()->GetSnake());
		setDisplayType( DT_SNAKE);
	}
	else
	{
		ImgView::DisplayType curDT = imgView(0)->getDisplayType();
		if ( curDT & DT_MAIN)
			setDisplayType( DT_MAIN);
		else
			setDisplayType( DT_SPEED);
	}
	redraw();
}

// Display menu - 'Show Segmentation'
void
Aseg::showSegmentation()
{
	if ( showSegmentationToggle() && haveSegmentation())
	{
		setDisplayType( DT_LABLE);
	}
	else
	{
		ImgView::DisplayType curDT = imgView(0)->getDisplayType();
		if ( curDT & DT_MAIN)
			setDisplayType( DT_MAIN);
		else
			setDisplayType( DT_SPEED);
	}
	redraw();
}

// Display menu - 'Show Fibrosis Threshold'
void
Aseg::showFibrosisThreshold()
{
	if ( _showFibrosisThresholdToggle->value() && imgView(_wallImgViewId)->labelImage())
	{
		setDisplayType( DT_FIB);
	}
	else
	{
		ImgView::DisplayType curDT = imgView(0)->getDisplayType();
		if ( curDT & DT_MAIN)
			setDisplayType( DT_MAIN);
		else
			setDisplayType( DT_SPEED);
	}
	redraw();
}

// Display menu - 'Show Pixel Value'
void
Aseg::showPixelValue()
{
	if ( _showPixelValueToggle->value())
		setViewMode( ImgView::VM_PIXVAL);
	else
		setViewMode( ImgView::VM_CROSS_HAIR);
	radioDisplay( _showPixelValueToggle);
}

// Display menu - 'Show XY Value'
void
Aseg::showXY()
{
	if ( _showXYToggle->value())
		setViewMode( ImgView::VM_POINT);
	else
		setViewMode( ImgView::VM_CROSS_HAIR);
	radioDisplay( _showXYToggle);
}

//
//	Analysis menu items
//
void
Aseg::doSegmentation()
{
	setCursor( XC_watch);

	message_window_timeout = 2;
	message( "Please wait for segmentation initialization");
	message_window_timeout = 0;

	// turn off registration if on
	registerMeshToggle()->value( false);
	meshView()->setViewMode( MeshView::VM_NONE);
	if ( _MeshWindowUI)
		_MeshWindowUI->meshView()->setViewMode( MeshView::VM_NONE);

	setStatus( "Please wait for segmentation initialization");
STIM(Timer tim;)

	if ( !segImage3D()->InOutPreprocessingDone() && !segImage3D()->EdgePreprocessingDone())
	{
		alert( "Error, must do Threshold or Edge processing for speed image first");
		return;
	}

	if ( !segmentationDlog())
		_SegmentationDlog = new SegmentationDlog( this);	// Segmentation Dialog

	segmentationDlog()->post( 0, SegmentationOkCB, SegmentationCancelCB, 0, this);
	segmentationDlog()->textOutput()->value( "Wait");	check();
	_snakeActive = true;

	if ( !resetSegmentation())
	{
		_snakeActive = false;
		segmentationDlog()->unpost();
		setCursor( XC_left_ptr);			// get rid of wait cursor
		return;
	}

	setSnakeImage( segImage3D()->GetSnake());			// m_SnakeWrapper->ImgViews
	setLabelImage( segImage3D()->GetSegmentation());	// m_LabelWrapper->ImgViews
	setViewMode( ImgView::VM_CROSS_HAIR);
	setDisplayType( DT_SNAKE);
	segmentationDlog()->textOutput()->value( "Ready");
	setStatus( "Ready to Segment");
	beep();
STIM(cout << "Snake init: " << tim.sec() << " sec" << endl;)

	setCursor( XC_left_ptr);			// get rid of wait cursor
	activateMenus();

	// Redraw the windows
	redraw();
}	// doSegmentation()

void
Aseg::registerModel()
{
	if ( _modelReg == 0)
		_modelReg = new ModelReg();

	const itk::Vector<double, 3u> spacing = segImage3D()->GetSpeed()->GetImage()->GetSpacing();
	_modelReg->setImage( segImage3D()->GetSpeed());
	Pt2<float> ctr = imgView(0)->centerPt();
	ctr = ctr * float(spacing[0]);
	double radius = imgView(0)->radius();
	radius *= spacing[0];
	_modelReg->initEllipse( radius, ctr.x(), ctr.y());
	_modelReg->Register();
}

bool
Aseg::getSegmentationVolume()
{
	unsigned short numslices;
	LabelImageWrapper3D::ImagePointer segimg;
	vector<Polygon<Pt2<float>, float> > polys;
	char errMsg[256];

	if ( segImage3D()->SegmentatonDone())
	{
		if ( segImage3D()->getSegmentationVolume( errMsg))
		{
			float volume = segImage3D()->getSegVolume();
			alert( "Volume = %d mm^3", int(volume));
		}
		else
			alert( errMsg);
		return true;
	}
	else if ( segImage3D()->hasWalls())
	{
		if ( segImage3D()->getSegmentationVolume( errMsg))
		{
			ostringstream oss;
			string wallNames[] = { "LaEndo", "RaEndo", "LvEndo", "RvEndo" };

			for ( uint w = 0; w < 4; w++)
			{
				float vol = segImage3D()->getWallVolume( w);
				if ( vol > float(0))
				{
					oss << wallNames[w] << " vol = " << int(vol) << " mm^3" << endl;
				}
			}
			alert( oss.str().c_str());
		}
	}
	else
		return false;

}	// getSegmentationVolume()

// couldn't use as local...
struct volStruct
{
	uint time;
	float volume;
};


//
//	Get volumes at all times.
//	Currently only for segmentation volume, not wall volumes.
//
void
Aseg::getAllSegmentationVolumes()
{
	vector<volStruct> volumes;
	uint saveImageTime = _imageTime;
	char errMsg[256];

	for ( uint i=0; i<_segImage3D.size(); i++)
	{
		_imageTime = i;
		if ( segImage3D()->SegmentatonDone())
		{
			struct volStruct vol;
			vol.time = segImage3D()->TriggerTime();
			if ( segImage3D()->getSegmentationVolume( errMsg))
			{
				vol.volume = segImage3D()->getSegVolume();
				volumes.push_back( vol);
			}
			else
				alert( errMsg);
		}
	}

	_fileChooser->type( FileChooser::CREATE);
	_fileChooser->label("Save Volumes");
	_fileChooser->filter( "*.vol");

	_fileChooser->show( 300, 300);

	while ( _fileChooser->visible())
		fltk::wait();

	if ( _fileChooser->count() > 0)
	{
		ofstream outFile;
		outFile.open( _fileChooser->value(), ios::out);
		if ( outFile)
		{
			outFile << "Time\tVolume" << endl;
			for ( vector<volStruct>::iterator vi = volumes.begin(); vi != volumes.end(); vi++)
			{
				outFile << (*vi).time << "\t" << (*vi).volume << endl;
			}
			outFile.close();
		}
	}

	_imageTime = saveImageTime;
}

//
//	Analyze myocardium to find fibrosis amount
//	wt: 1 - LaEndo, 2 - RaEndo, 3 - LvEndo, 4 - RvEndo wall
//
//	Operates on all slices with walls.
//	Called from Operation/Fibrosis
//
void
Aseg::analyzeFibrosis( uint wt)
{
	string endoWall[4] = {"La", "Ra", "Lv", "La"};

	// get ImgView id for walls
	for ( uint i=0; i<3; i++)
		if ( imageAxis(i) == 2)
		{
			_wallImgViewId = i;
			break;
		}

	if ( !segImage3D()->IsThreshLoaded())
		segImage3D()->SetThreshImage( getGeometry());

	segImage3D()->getMyoPixels( wt, _wallImgViewId, _fibrosisPixels);

	cout << "Aseg::analyzeFibrosis() " << endoWall[wt-1] << "Endo , num pixels = "
		<< _fibrosisPixels.size() << endl;

	if ( _fibrosisPixels.size() == 0)
	{
		alert( "Error, no myo pixels found");
		return;
	}

	// get and display and analyze histogram
	if ( _fibrosisHistogram == 0)
		_fibrosisHistogram = new FibrosisHistogram();

	_fibrosisHistogram->post( 0, FibrosisOkCB, 0, FibrosisApplyCB, this);
	_fibrosisHistogram->_numBinsChoice->value(1);		// 128
	_fibrosisHistogram->setPixels( &_fibrosisPixels);
	_fibrosisHistogram->getHistogram();

	activateMenus();
}

//
//	Image scrollbar change call
//
void
Aseg::sliceChange()
{
	if ( !image()->GetGrey()->IsInitialized())
		return;

	for ( uint i=0; i<3; i++)
	{
		uint val = getSlice(i);
//		cout << "sliceChange: dim " << i << ", min " << _sliceScrollbar[i]->minimum() <<
//		" - value " << _sliceScrollbar[i]->value() << " = " << val << endl;
		_cursor[ _imageAxes[i]] = val;
	}
//	cout << "Aseg: slices " << _cursor[0] << ", " << _cursor[1] << ", " << _cursor[2] << endl;
	// make sure ImgWindowUI Scrollbar in sync
	if ( _ImgWindowUI && _ImgWindowUI->_imgWindow->visible())
		setWinSlice( getSlice( _ImgWindowUI->_winImgView->getId()));

	image()->GetGrey()->SetSliceIndex( _cursor);
	if ( segImage3D()->NativeImageIsLoaded())
	{
		if ( mainROI() == segROI())
			segImage3D()->GetGrey()->SetSliceIndex( _cursor);
		if ( segImage3D()->IsSpeedLoaded())				// threshold
		{
			segImage3D()->GetSpeed()->SetSliceIndex( _cursor);
			if ( _thresholdFilter->isPosted() && _thresholdFilter->showPreview())
				_thresholdFilter->thresholdPreviewUpdate();		// to get new slice
			if ( _edgeFilter->isPosted() && _edgeFilter->showPreview())
				_edgeFilter->EdgePreviewUpdate();		// to get new slice
		}
		if ( segImage3D()->IsSnakeLoaded())				// level set image
			segImage3D()->GetSnake()->SetSliceIndex( _cursor);
		if ( segImage3D()->IsSegmentationLoaded())		// gipl
			segImage3D()->GetSegmentation()->SetSliceIndex( _cursor);
		if ( segImage3D()->IsThreshLoaded())			// fibrosis
			segImage3D()->GetThreshImage()->SetSliceIndex( _cursor);
		if ( segImage3D()->isShapeLoaded())				// initial shape image
			segImage3D()->GetShape()->SetSliceIndex( _cursor);
	}

	activateEditMenu();
	redraw();
}

//
//	Time slider	!!!fixme: probably needs work 02-apr-15
//
void
Aseg::setTime()
{
	_imageTime = (uint)_timeSlider->value() - 1;

	if ( mainROI() != segROI())
	{
		setImage( segImage3D()->GetGrey(), segImage3D()->PixelSpacing());
		_image = segImage3D();
		if ( showSnakeToggle() && segImage3D()->IsSnakeLoaded())
		{
			setSnakeImage( segImage3D()->GetSnake());
			showSnake();
			updateSegMesh( true);
		}

		_showSnakeToggle->value(false);
		segMesh()->Reset();
		setDisplayType( DT_MAIN);
	}
	else
	{
		setImage( mainImage()->GetGrey(), mainImage()->PixelSpacing());
		_image = mainImage();
		setDisplayType( DT_MAIN);
	}

	ostringstream oss;
	oss << "Time ";
	oss << image()->TriggerTime();
	setStatus( oss.str().c_str());

	sliceChange();		// does redraw()
	activateMenus();
}

void
Aseg::showImgWindow( uint i)
{
	if ( _ImgWindowUI == 0)
	{
		_ImgWindowUI = new ImgWindowUI;
		_ImgWindowUI->make_window();
		_ImgWindowUI->parent( this);
		_ImgWindowUI->_winImgView->setParent( this);
	}
	_ImgWindowUI->_imgWindow->show();
	_ImgWindowUI->_winImgView->show();
	_ImgWindowUI->_winImgView->setId( i);
	if ( _mainImage.size())
	{
		if ( mainROI() != segROI())
			_ImgWindowUI->_winImgView->setImage(
				segImage3D()->GetGrey(), segImage3D()->PixelSpacing());
		else
			_ImgWindowUI->_winImgView->setImage(
				mainImage()->GetGrey(), mainImage()->PixelSpacing());

		if ( imgView(i)->speedImage())
			_ImgWindowUI->_winImgView->setSpeedImage( segImage3D()->GetSpeed()); //,
//				segImage3D()->PixelSpacing());
		if ( imgView(i)->speedImage2D())
			_ImgWindowUI->_winImgView->setSpeedImage2D( segImage3D()->GetSpeed2D()); //,
		if ( imgView(i)->initShapeImg())
			_ImgWindowUI->_winImgView->setInitShapeImg( 0);	//?
		if ( imgView(i)->snakeImage())
			_ImgWindowUI->_winImgView->setSnakeImage( segImage3D()->GetSnake());
		if ( imgView(i)->labelImage())
			_ImgWindowUI->_winImgView->setLabelImage( imgView(i)->labelImage());
		initWinSlider();
		sliceChange();
		_ImgWindowUI->_winImgView->setMode( imgView(i)->getMode());
		_ImgWindowUI->_winImgView->setDisplayType( imgView(i)->getDisplayType());
	}
}

void
Aseg::showMeshWindow()
{
	if ( _MeshWindowUI == 0)
	{
		_MeshWindowUI = new MeshWindowUI;
		_MeshWindowUI->make_window();
		_MeshWindowUI->parent( this);
	}
	_MeshWindowUI->_meshWindow->show();
	_MeshWindowUI->meshView()->show();
	if ( _mainImage.size())
	{
		if ( mainROI() != segROI())
			_MeshWindowUI->meshView()->setImage( segImage3D()->GetGrey());
		else
			_MeshWindowUI->meshView()->setImage( mainImage()->GetGrey());
	}
	_MeshWindowUI->meshView()->initialize( this, mesh());
	_MeshWindowUI->meshView()->segMesh( segMesh());
	_MeshWindowUI->meshView()->regMesh( regMesh());
	updateMeshToggles();
	showMesh();
	showRegisteredMesh();
}

//**********************************************************************
//
//	public functions
//
//**********************************************************************

//
//	Called from ImgView::updateRoi()
//
void
Aseg::setRoi( RegionType &roi)
{
	_segROI = roi;
	for ( uint i=0; i<3; i++)
		_imgView[i]->resetRoi();
}

//
//	Called from Aseg::loadROI() & ImgView::handle() case VM_CROSS_HAIR
//
void
Aseg::setCrosshairs( Pt3<float> &pt)
{
	itk::Size<3u> sz = segROI().GetSize();

	for ( uint dim = 0; dim < 3; dim++)
	{
		float val = pt[dim];
		if ( val < float(0))
			val = float(0);
		if ( val >= sz[dim])
			val = sz[dim] - 1;
		_cursor[dim] = uint(val);
	}

	// update slice scrollbars
	for ( uint dim = 0; dim < 3; dim++)
	{
		uint slice = _cursor[_imageAxes[dim]];
		setSlice(dim, double( slice));
	}

	sliceChange();
}

//
//	Called from ThresholdFilter & EdgeFilter
//
void
Aseg::preprocessingPreview( bool preview)
{
	if ( preview)
	{
		setDisplayType( DT_SPEED);
	}
	else
	{
		setDisplayType( DT_MAIN);
	}
	redraw();
}

//
//	segmentation thread function
//
void *runSegmentation( void *arg)
{
	Aseg *obj = (Aseg *)arg;
	unsigned int step = obj->segmentationDlog()->getStepSize();
	while ( obj->snakeRunning() == true)
	{
		if ( obj->_segUpdateDone)
		{
			obj->segImage3D()->RunSegmentation( step);
			obj->_segUpdateDone = false;
		}
	}
	return 0;
}

//
//	called from SegmentatonDlog
//
void
Aseg::restartSegmentation()
{
	stopSegmentation();						// stops thread, updates mesh

	segImage3D()->RestartSegmentation();
	_snakeActive = true;
	snakeUpdate();				// updates mesh
	segMesh()->Reset();

	segTime_.init();
	segmentationDlog()->setIterations( 0);
	segmentationDlog()->setSeconds( 0);
	redraw();
}

//
//	Set in startSegmentation()
//
void
SnakeIdleFunction( void *userData)
{
	Aseg *obj = (Aseg *)userData;

	obj->snakeUpdate();		// updates mesh
}

//
//	called from SegmentatonDlog
//
void Aseg::startSegmentation()
{
	if ( _snakeRunning)
		return;

	segTime_.init();
	segmentationDlog()->setSeconds( 0);
	setStatus( "Segmentation running");	flush();
	fltk::add_idle( SnakeIdleFunction, this);

	int id;
	int errcode;
	_segUpdateDone = true;
	_snakeRunning = true;
	if ( errcode = pthread_create( &_thread, 0, runSegmentation, this))
	{
		alert( "pthread_create error: %s", strerror(errcode));
	}
	cout << "Aseg::startSegmentation(): thread " << _thread << endl;
}

//
//	called from SegmentatonDlog
//
void Aseg::stopSegmentation( bool updateMesh)		// updateMesh default true
{
	_snakeActive = false;
	if ( !_snakeRunning)
	{
		setStatus( "Canceled segmentation");
		return;
	}
	int errcode;
	int *status;

	setStatus( "Stopping segmentation");	flush();

	cout << "Aseg::stopSegmentation(): thread " << _thread << endl;
	_snakeRunning = false;			// need here for join to work
	if ( errcode = pthread_join( _thread, (void **)&status))
	{
		alert( "pthread_join error: %s", strerror(errcode));
	}
	fltk::remove_idle( SnakeIdleFunction, this);

	_segUpdateDone = false;
	snakeUpdate( updateMesh);

	segmentationDlog()->textOutput()->value( "Stopped");
	setStatus( "Segmentation stopped");	flush();
}

//
//	called from SegmentatonDlog
//
void Aseg::stepSegmentation()
{
	stopSegmentation();			// updates mesh

	segTime_.init();
	_snakeActive = true;
	segImage3D()->RunSegmentation( segmentationDlog()->getStepSize());
	_segUpdateDone = false;			// need false or snakeUpdate() just returns
	snakeUpdate();				// updates mesh
	_snakeActive = false;
}

//
//	Called when segmentation done (segmentationOK()).
//	Based on Snap IRISApplication::UpdateIRISWithSnapImageData().
//
void
Aseg::updateSegmentationData()
{
	// Get pointers to the source and destination images
	typedef LevelSetImageWrapper3D::ImageType SourceImageType;
	typedef LabelImageWrapper3D::ImageType TargetImageType;

	// If the voxel size of the image does not match the voxel size of the
	// main image, we need to resample the region
	SourceImageType::Pointer source = segImage3D()->GetSnake()->GetImage();	// m_SnakeWrapper
	TargetImageType::Pointer target = segImage3D()->GetSegmentation()->GetImage();
																			// m_LabelWrapper

	// Construct region of interest into which the result will be pasted
	// SNAPSegmentationROISettings roi = m_GlobalState->GetSegmentationROISettings();
	LabelImageType::RegionType region = target->GetBufferedRegion();

	// Create iterators for copying from one to the other
	typedef itk::ImageRegionConstIterator<SourceImageType> SourceIteratorType;
	typedef itk::ImageRegionIterator<TargetImageType> TargetIteratorType;
	SourceIteratorType itSource( source, source->GetLargestPossibleRegion());
	TargetIteratorType itTarget( target, region);	// roi.GetROI());

	// Figure out which color draws and which color is clear
	unsigned int iClear = 0;	// m_GlobalState->GetPolygonInvert() ? 1 : 0;

	// Construct a merge table that contains an output threshold for every
	// possible combination of two input thresholds (note that snap image only
	// has two possible thresholds)
	LabelTypeUS mergeTable[2][MAX_COLOR_LABELS];

	// Perform the merge
	for(unsigned int i=0;i<MAX_COLOR_LABELS;i++)
	{
		// When the SNAP image is clear, IRIS passes through to the output
		// except for the IRIS voxels of the drawing color, which get cleared out
		mergeTable[iClear][i] = (i!= GetDrawingColorLabel()) ? i : 0;

		// If mode is paint over all, the victim is overridden
		mergeTable[1-iClear][i] = DrawOverLabel((LabelTypeUS) i);
	}

	// Go through both iterators, copy the new over the old
	itSource.GoToBegin();
	itTarget.GoToBegin();
	while(!itSource.IsAtEnd())
	{
		// Get the two voxels
		LabelTypeUS &voxIRIS = itTarget.Value();
		float voxSNAP = itSource.Value();

		// Check that we're ok (debug mode only)
		assert(!itTarget.IsAtEnd());

		// Perform the merge
		voxIRIS = mergeTable[voxSNAP <= 0 ? 1 : 0][voxIRIS];

		// Iterate
		++itSource;
		++itTarget;
	}

	// The target has been modified
	target->Modified();
}

//
//	Called from Aseg::updateSegmentationData()
//
LabelTypeUS
Aseg::DrawOverLabel( LabelTypeUS iTarget)
{
	// Get the current merge settings
	uint iMode = PAINT_OVER_ALL;	// m_GlobalState->GetCoverageMode();
	LabelTypeUS iDrawing = GetDrawingColorLabel();
	LabelTypeUS iDrawOver = 0;		// m_GlobalState->GetOverWriteColorLabel();

	// Assign the output threshold based on the current drawing mode
	bool visible = GetColorLabelTable()->GetColorLabel( iTarget).IsVisible();

	// If mode is paint over all, the victim is overridden
	return
		((iMode == PAINT_OVER_ALL) ||
		(iMode == PAINT_OVER_COLORS && visible) ||
		(iMode == PAINT_OVER_ONE && iDrawOver == iTarget)) ? iDrawing : iTarget;
}

//
//	called from SegmentationOkCB()
//
void
Aseg::segmentationOK()
{
	setCursor( XC_watch);
	setStatus( "Terminating segmentation");
	stopSegmentation( false);					// stops thread, no mesh update yet
	segImage3D()->LevelSetDriver()->Print();

	segImage3D()->TerminateSegmentation();		// deletes LevelSetDriver
	updateSegmentationData();

	updateSegMesh( true);
	setStatus( "Segmentation done");

	cout << "Time:              " << segmentationDlog()->getSeconds() << " sec" << endl;

	setCursor( XC_left_ptr);			// get rid of wait cursor
	activateMenus();
	setDisplayType( DT_LABLE);

	// Redraw the windows
	redraw();
}

//
//	called from SegmentationCancelCB()
//
void
Aseg::segmentationCancel()
{
	stopSegmentation();						// stops thread, updates mesh
	segImage3D()->TerminateSegmentation();
}

//
//	called from ImageIntensity UI, Edit/Intensity
//
void
Aseg::intensityChanged()
{
	image()->GetGrey()->RemapIntensityToRange(
		_intensityFilter->intensityMin(), _intensityFilter->intensityMax());
	redraw();
}

void
Aseg::intensityOk()
{
}

void
Aseg::intensityCancel()
{
	image()->GetGrey()->RemapIntensityToMaximumRange();
	redraw();			// redraw this window
}

//
//	Called from sliceChange() and ImgView::handle() VM_EDIT_WALL->MOVE
//
void
Aseg::activateEditMenu()
{
	if ( segImage3D()->wallSelected())
	{
		_editMenu->child( DELETE_WALL)->activate();
	}
	else
	{
		_editMenu->child( DELETE_WALL)->deactivate();
	}
	if ( segImage3D()->hasPrevWalls())
		_editMenu->child( COPY_PREV_WALLS)->activate();
	else
		_editMenu->child( COPY_PREV_WALLS)->deactivate();
	if ( segImage3D()->hasNextWalls())
		_editMenu->child( COPY_NEXT_WALLS)->activate();
	else
		_editMenu->child( COPY_NEXT_WALLS)->deactivate();
}

void
Aseg::redraw()
{
	for ( uint i=0; i<3; i++)
		imgView(i)->redraw();

	if ( _ImgWindowUI && _ImgWindowUI->_imgWindow->visible())
		_ImgWindowUI->_winImgView->redraw();

	meshView()->redraw();

	if ( _MeshWindowUI && _MeshWindowUI->_meshWindow->visible())
		_MeshWindowUI->meshView()->redraw();
}

//
//	Called from MeshView::rotateMesh(), Aseg::clearSegmentation().
//	Apply MeshView's modTrans/RotMatrix's to Mesh.
//
void
Aseg::initRegMesh()
{
	if ( !regMesh())
		return;

	float zoom = meshView()->modZoom();

	uint n = regMesh()->GetNumberOfVTKMeshes();
	for ( uint wt=0; wt < n; wt++)
	{
		if ( displayMesh( wt))
		{
			TransMatrix tm = *meshView()->modTransMatrix( wt);
			TransMatrix *rm = meshView()->modRotMatrix( wt);		// pointer

			if ( zoom != 1.0F)
				regMesh()->resizeMesh( double(zoom), wt);

			if ( !tm.isIdentity())
				regMesh()->translateMesh( tm[3], wt);				// tm is col-major

			if ( !rm->isIdentity())
			{
				Pt3<float> ctr =  regMesh()->getBbox().center();
				regMesh()->rotateMesh( *rm, ctr, wt);
			}
		}
	}
	meshView()->resetRegistration();
	if ( _MeshWindowUI)
		_MeshWindowUI->meshView()->resetRegistration();
	regMesh()->GenerateDisplayLists();
}

//
//	ImgWindow scrollbar change call.  Only public _ImgWindowUI function.
//	Keep _imgView[_ImgWindowUI->_winImgView->getId()] scrollbar in sync.
void
Aseg::winSliceChange( double val)
{
	val = _ImgWindowUI->_sliceScrollbar->minimum() - val;
	setSlice( _ImgWindowUI->_winImgView->getId(), val);
	sliceChange();
}

//	public because also used from MeshWindowUI (as well as AsegUI)
void
Aseg::showAxial()
{
	_showAxial = !_showAxial;
	meshView()->redraw();
	if ( _MeshWindowUI)
		_MeshWindowUI->_meshWindow->redraw();
}

void
Aseg::showSagital()
{
	_showSagital = !_showSagital;
	meshView()->redraw();
	if ( _MeshWindowUI)
		_MeshWindowUI->_meshWindow->redraw();
}

void
Aseg::showCoronal()
{
	_showCoronal = !_showCoronal;
	meshView()->redraw();
	if ( _MeshWindowUI)
		_MeshWindowUI->_meshWindow->redraw();
}

// MeshView buttons.  MeshWindowUI (as well as AsegUI)
void
Aseg::rotate90( uint dim)
{
	meshView()->rotate90( dim);
	if ( _MeshWindowUI)
		_MeshWindowUI->meshView()->rotate90( dim);
}

// Display menu item and 3D view button.  MeshWindowUI (as well as AsegUI)
void
Aseg::reset3D()
{
	meshView()->reset();			// ModelView::reset()
	if ( _MeshWindowUI)
		_MeshWindowUI->meshView()->reset();
}

//
//	Other private functions
//

//
//	Called from loadROI() and setROI()
//	See Logic/Framework/IRISApplication.cxx::InitializeSNAPImageData()
//
void
Aseg::initROI()
{
	progress()->label( "Setting ROI image");
	progress()->show();

	// clear any existing ImgView images
	setInitShapeImg( 0);
	setSpeedImage( 0);
	setSnakeImage( 0);
//?	setLabelImage( 0);
	_thresholdSettingsLoaded = false;

	//	Initialize the segImage for segmentation
	initSegImage();

	// UI stuff
	setImage( segImage3D()->GetGrey(), segImage3D()->PixelSpacing());	// Sets _sliceSize[]
																		// which determines ROI
	_image = segImage3D();
	setLabelImage( segImage3D()->GetSegmentation());

	initSliders();
	setDisplayType( DT_MAIN);
	sliceChange();

	progress()->hide();

	// rotate cube a little
//	meshView()->rotate( float(20), float(20));

	_setRoiButton->deactivate();
	_resetRoiItem->activate();
}

//
//	Initialize the segImage for segmentation.
//	Called from loadImages(), loadSegmentation, initROI(), resetROI().
//
void
Aseg::initSegImage()
{
	string DisplayToAnatomyRAI[3] = {"RPS", "AIL", "RIP"};
	::LabelTypeUS passThroughLabel = GetDrawingColorLabel();

	for ( uint i=0; i < _mainImage.size(); i++)
	{
		if ( mainROI() != segROI())
		{
			// Get the roi chunk from the grey image
			typedef itk::Image<GreyType,3> GreyImageType;
			GreyImageType::Pointer imgNewGrey;
			if ( _mainImage.size() == 1)
				imgNewGrey = _mainImage[i]->GetGrey()->DeepCopyRegion( segROI(), 0);
			else
			{
				imgNewGrey = _mainImage[i]->GetGrey()->DeepCopyRegion( segROI(),
									progress()->GetRedrawCommand());
				progress()->position( (double(i) * 100.0) / double(_mainImage.size()));
				progress()->redraw();	check();
			}
			// Get the size of the region
			Vector3ui size = to_unsigned_int(
				Vector3ul( imgNewGrey->GetLargestPossibleRegion().GetSize().GetSize()));

			// Compute an image coordinate geometry for the region of interest
			ImageCoordinateGeometry icg(
				_ImageCoordinateGeometry.GetImageDirectionCosineMatrix(),
				DisplayToAnatomyRAI, size);

			// Assign the new wrapper to the target
			_segImage3D[i]->SetGreyImage( imgNewGrey, icg,
				_mainImage[i]->GetGrey()->GetNativeMapping());

			// Set segmentation label image
			// Override the interpolator in ROI for label interpolation, or will get nonsense
			//	SegmentationROISettings roiLabel;
			//	roiLabel.SetROI( segROI());
			//	roiLabel.SetInterpolationMethod(SegmentationROISettings::NEAREST_NEIGHBOR);

			// Get	the label image
			LabelImageType::Pointer imgNewLabel;
			if ( _segImage)
			{	// resize it
				RoiFilterType::Pointer ROIfilter = RoiFilterType::New();
				ROIfilter->SetInput( _segImage);
				ROIfilter->SetRegionOfInterest( segROI());
				ROIfilter->Update();
				_segROIImage = ROIfilter->GetOutput();

				imgNewLabel = _segROIImage;
			}
			else
				imgNewLabel = _segImage3D[i]->GetSegmentation()->GetImage();

			unsigned short imin, imax;
			GetMinMax<LabelImageType, LabelTypeUS>( imgNewLabel, imin, imax, "seg before");

			// only filter if haven't loaded segmentation
			if ( !_segImage)
			{
				// Filter the segmentation image to only allow voxels of 0 threshold and
				// of the current drawing color
				typedef itk::ImageRegionIterator<LabelImageType> IteratorType;
				IteratorType itLabel( imgNewLabel, imgNewLabel->GetBufferedRegion());
				while ( !itLabel.IsAtEnd())
				{
					if ( itLabel.Value() != passThroughLabel)
						itLabel.Value() = (LabelTypeUS) 0;
					++itLabel;
				}
				GetMinMax<LabelImageType, LabelTypeUS>( imgNewLabel, imin, imax, "seg after");
			}

			// Pass the cleaned up segmentation image to SNAP
			_segImage3D[i]->SetSegmentationImage( imgNewLabel);
			if ( _segImage)
				_segROIImage = imgNewLabel;
		}
		else
		{
			_segImage3D[i]->GetSegmentation()->GetImageBase()->SetSpacing(
				_segImage3D[i]->GetGrey()->GetImageBase()->GetSpacing());
			_segImage3D[i]->GetSegmentation()->GetImageBase()->SetOrigin(
				_segImage3D[i]->GetGrey()->GetImageBase()->GetOrigin());
		}

		// Pass the label description of the drawing label to the SNAP image data
		_segImage3D[i]->SetColorLabel( GetColorLabelTable()->GetColorLabel(passThroughLabel));

		// initialize the speed image
		_segImage3D[i]->InitializeSpeed();

		// UserInterfaceLogic::UpdateSpeedColorMap()
		_segImage3D[i]->GetSpeed()->SetColorMap(
//			SpeedColorMap::GetPresetColorMap( COLORMAP_BLUE_BLACK_WHITE));
			SpeedColorMap::GetPresetColorMap( COLORMAP_BLACK_BLACK_WHITE));
	}
}	// initSegImage()

template< typename ImgType, typename PixType>
void
Aseg::GetMinMax( ImgType *img, PixType &min, PixType &max, const char *msg)
{
	typedef itk::MinimumMaximumImageCalculator<ImgType> MinMaxCalculatorType;
	typename MinMaxCalculatorType::Pointer MinMaxCalculator =  MinMaxCalculatorType::New();
	MinMaxCalculator->SetImage( img);
	MinMaxCalculator->Compute();

	min = MinMaxCalculator->GetMinimum();
	max = MinMaxCalculator->GetMaximum();

	if ( msg)
		cout << msg << ": " << min << ", " << max << endl;
}

void
Aseg::activateMenus()
{
	if ( _mainImage.size() == 0)
		return;
	if ( segImage3D()->GetGrey()->IsInitialized())
	{
		_fileMenu->child( LOAD_ROI)->activate();
		_fileMenu->child( LOAD_BUBBLES)->activate();
		_fileMenu->child( LOAD_SEGMENTATION)->activate();
		_fileMenu->child( LOAD_WALLS)->activate();
		_fileMenu->child( SAVE_ROI)->activate();
		_editMenu->child( ADD_WALL)->activate();
		if ( _setRoiButton->value())
		{
			_showPixelValueToggle->deactivate();
			_showXYToggle->deactivate();
		}
		else
		{
			_showPixelValueToggle->activate();
			_showXYToggle->activate();
		}
	}
	else
	{
		_fileMenu->child( LOAD_ROI)->deactivate();
		_fileMenu->child( LOAD_SEGMENTATION)->deactivate();
		_fileMenu->child( SAVE_ROI)->deactivate();
		_fileMenu->child( LOAD_WALLS)->deactivate();
		_editMenu->child( ADD_WALL)->deactivate();
		_editMenu->child( COPY_PREV_WALLS)->deactivate();
		_editMenu->child( COPY_NEXT_WALLS)->deactivate();
		_showPixelValueToggle->deactivate();
	}

	if ( haveBubbles())
	{
		_fileMenu->child( SAVE_BUBBLES)->activate();
	}
	else
	{
		_fileMenu->child( SAVE_BUBBLES)->deactivate();
	}

	if ( regMesh())
		_fileMenu->child( SAVE_INIT_MESH)->activate();
	else
		_fileMenu->child( SAVE_INIT_MESH)->deactivate();

	if ( segMesh()->GetNumberOfVTKMeshes())
		_fileMenu->child( SAVE_SEG_MESH)->activate();
	else
		_fileMenu->child( SAVE_SEG_MESH)->deactivate();

	if ( (haveThresholdSettings() || haveEdgeSettings())
			&& ( haveBubbles() || mesh()->GetNumberOfVTKMeshes()))
		_analysisMenu->child( SEGMENT)->activate();
	else
		_analysisMenu->child( SEGMENT)->deactivate();

	if ( imgView(0)->speedImage())
		_displayMenu->child( SHOW_SPEED)->activate();
	else
		_displayMenu->child( SHOW_SPEED)->deactivate();

	if ( segImage3D()->IsSegmentationLoaded())	// SegImage3D m_LabelWrapper
	{
//		if ( _setRoiButton->value() || snakeActive())	// 24-sep-15 not sure
//			_displayMenu->child( SHOW_SEG)->deactivate();
//		else
//			_displayMenu->child( SHOW_SEG)->activate();
		_displayMenu->child( REG_MESH)->activate();
		_displayMenu->child( RESET_REG)->activate();
		_analysisMenu->child( REG_MODEL)->activate();
		_analysisMenu->child( SEG_VOLUME)->activate();
		_analysisMenu->child( ALL_SEG_VOLUME)->activate();
	}
	else
	{
		_displayMenu->child( REG_MESH)->deactivate();
		_displayMenu->child( RESET_REG)->deactivate();

		_analysisMenu->child( REG_MODEL)->deactivate();
		_analysisMenu->child( SEG_VOLUME)->deactivate();
		_analysisMenu->child( ALL_SEG_VOLUME)->deactivate();
	}

	if ( haveSegmentation())	// ImgView _labelImage
	{
		_fileMenu->child( SAVE_SEGMENTATION)->activate();
		_editMenu->child( CLEAR_SEGMENTATION)->activate();
		_displayMenu->child( SHOW_SEG)->activate();
	}
	else
	{
		_fileMenu->child( SAVE_SEGMENTATION)->deactivate();
		_editMenu->child( CLEAR_SEGMENTATION)->deactivate();
		_displayMenu->child( SHOW_SEG)->deactivate();
	}

	if ( segImage3D()->IsSnakeLoaded())
		_displayMenu->child( SHOW_SNAKE)->activate();
	else
		_displayMenu->child( SHOW_SNAKE)->deactivate();

	if ( segImage3D()->hasWalls())
	{
		_fileMenu->child( SAVE_WALLS)->activate();
		_editMenu->child( EDIT_WALL)->activate();
		_analysisMenu->child( SEG_VOLUME)->activate();
		_analysisMenu->child( FIBROSIS)->activate();
	}
	else
	{
		_fileMenu->child( SAVE_WALLS)->deactivate();
		_editMenu->child( EDIT_WALL)->deactivate();
		_editMenu->child( DELETE_WALL)->deactivate();
//		_analysisMenu->child( SEG_VOLUME)->deactivate();
		_analysisMenu->child( FIBROSIS)->deactivate();
	}

	if ( segImage3D()->IsThreshLoaded())
		_showFibrosisThresholdToggle->activate();
	else
		_showFibrosisThresholdToggle->deactivate();

	if ( _newStartOps.size())
		_fileMenu->child( SAVE_STARTOPS)->activate();
	else
		_fileMenu->child( SAVE_STARTOPS)->deactivate();
}	// activateMenus()

void
Aseg::setImage( GreyImageWrapper3D *wrapper, vector<float> spacing)
{
	for ( uint i=0; i<3; i++)
		imgView(i)->setImage( wrapper, spacing);		// calls invalidate()

	if ( _ImgWindowUI)
		_ImgWindowUI->_winImgView->setImage( wrapper, spacing);

	meshView()->setImage( wrapper);
	if ( wrapper)
	{
		meshView()->initialize( this, mesh());
		meshView()->segMesh( segMesh());
	}
	if ( _MeshWindowUI)
	{
		_MeshWindowUI->meshView()->setImage( wrapper);
		if ( wrapper)
		{
			_MeshWindowUI->meshView()->initialize( this, mesh());
			_MeshWindowUI->meshView()->segMesh( segMesh());
		}
	}
}

void
Aseg::setSpeedImage( SpeedImageWrapper3D *wrapper)
{
	for ( uint i=0; i<3; i++)
		imgView(i)->setSpeedImage( wrapper);

	if ( _ImgWindowUI)
		_ImgWindowUI->_winImgView->setSpeedImage( wrapper);
}

void
Aseg::setSpeedImage2D( SpeedImageWrapper *wrapper)
{
	for ( uint i=0; i<3; i++)
		imgView(i)->setSpeedImage2D( wrapper);

	if ( _ImgWindowUI)
		_ImgWindowUI->_winImgView->setSpeedImage2D( wrapper);
}

void
Aseg::setInitShapeImg( LabelImageWrapper3D *wrapper)
{
	for ( uint i=0; i<3; i++)
		imgView(i)->setInitShapeImg( wrapper);

	if ( _ImgWindowUI)
		_ImgWindowUI->_winImgView->setInitShapeImg( wrapper);
}

void
Aseg::setSnakeImage( LevelSetImageWrapper3D *wrapper)
{
	for ( uint i=0; i<3; i++)
		imgView(i)->setSnakeImage( wrapper);

	if ( _ImgWindowUI)
		_ImgWindowUI->_winImgView->setSnakeImage( wrapper);
}

void
Aseg::setLabelImage( LabelImageWrapper3D *wrapper)
{
	for ( uint i=0; i<3; i++)
		imgView(i)->setLabelImage( wrapper);

	if ( _ImgWindowUI)
		_ImgWindowUI->_winImgView->setLabelImage( wrapper);
}

void
Aseg::setImgViewMesh()
{
	for ( uint i=0; i<3; i++)
	{
		imgView(i)->mesh( mesh());
		imgView(i)->regMesh( regMesh());
		imgView(i)->segMesh( segMesh());
	}

	if ( _ImgWindowUI)
	{
		_ImgWindowUI->_winImgView->mesh( mesh());
		_ImgWindowUI->_winImgView->regMesh( regMesh());
		_ImgWindowUI->_winImgView->segMesh( segMesh());
	}
}

void
Aseg::setViewMode( ImgView::ViewMode m)
{
	for ( uint i=0; i<3; i++)
		imgView(i)->setMode( m);

	if ( _ImgWindowUI)
		_ImgWindowUI->_winImgView->setMode( m);
}

//
//	Set ImgView's DisplayType.  Enforce main/speed radio and snake,seg,fib radio.
//
void
Aseg::setDisplayType( ImgView::DisplayType dt)
{
	ImgView::DisplayType curDT = imgView(0)->getDisplayType();
	curDT = curDT & ~(DT_SNAKE | DT_LABLE | DT_FIB | DT_INIT | DT_BUBBLES);	// clear these

	switch ( dt)
	{
		case DT_MAIN:
			_showSpeedToggle->value( false);	// radio MAIN & SPEED
			curDT = curDT & ~DT_SPEED;
			curDT = curDT |= DT_MAIN;
			break;
		case DT_SPEED:
			_showSpeedToggle->value( true);		// radio MAIN & SPEED
			curDT = curDT & ~DT_MAIN;
			curDT = curDT |= DT_SPEED;
			break;
		case DT_SNAKE:
			displayRadio( _showSnakeToggle);
			break;
		case DT_LABLE:
			displayRadio( _showSegmentationToggle);
			break;
		case DT_FIB:
			displayRadio( _showFibrosisThresholdToggle);
			break;
		case DT_INIT:
			displayRadio( _showInitShapeToggle);
			break;
		case DT_BUBBLES:
			curDT = curDT |= DT_BUBBLES;
			break;
		default:
			break;
	}
	// set if any true
	curDT |= _showSnakeToggle->value() ? DT_SNAKE : 0;
	curDT |= _showSegmentationToggle->value() ? DT_LABLE : 0;
	curDT |= _showFibrosisThresholdToggle->value() ? DT_FIB : 0;
	curDT |= _showInitShapeToggle->value() ? DT_INIT : 0;
	curDT |= showBubblesToggle() ? DT_BUBBLES : 0;

	for ( uint i=0; i<3; i++)
		imgView(i)->setDisplayType( curDT);

	if ( _ImgWindowUI)
		_ImgWindowUI->_winImgView->setDisplayType( curDT);
}

//	Radio behavior for Display: Show Snake, Show Segmentation, Show Fibrosis
//	Enforce radio behavior because Fltk 2.0 doesn't unless have in group
void
Aseg::displayRadio( fltk::LightButton *b)
{
	_showSnakeToggle->value( _showSnakeToggle == b);
	_showSegmentationToggle->value( _showSegmentationToggle == b);
	_showFibrosisThresholdToggle->value( _showFibrosisThresholdToggle == b);
	_showInitShapeToggle->value( _showInitShapeToggle == b);
}

//	Radio behavior for Display: Show Pixel & Show XY
void
Aseg::radioDisplay( LightButton *button)
{
	_showXYToggle->value( button == _showXYToggle);
	_showPixelValueToggle->value( button == _showPixelValueToggle);
}

void
Aseg::centerChildWindow( Window *childWin)
{
	int px = _window->x() + (_window->w() - childWin->w()) / 2;
	int py = _window->y() + (_window->h() - childWin->h()) / 2;
	childWin->resize( px, py, childWin->w(), childWin->h());
}

void
Aseg::setCursor( int ncur)
{
	::setCursor( window(), ncur);
	for ( uint i=0; i<3; i++)
		::setCursor( imgView(i), ncur);
	if ( _ImgWindowUI && _ImgWindowUI->_imgWindow->visible())
		::setCursor( _ImgWindowUI->_winImgView, ncur);

	::setCursor( meshView(), ncur);
	if ( _MeshWindowUI)
		::setCursor( _MeshWindowUI->meshView(), ncur);

}

void
Aseg::initSliders()
{
	Vec3<uint> size = image()->GetGrey()->GetSize();
	for ( uint i=0; i<3; i++)
	{
		double val = double(size[_imageAxes[i]]);
		_sliceScrollbar[i]->range( val, 1.0);			// reverse range
		setSlice(i, val/2);
		_sliceScrollbar[i]->linesize( 1.0);
	}
	initWinSlider();
}

//
// ImgWindow only function.  Called from showImgWindow(), initSliders().
//
void
Aseg::initWinSlider()
{
	Vec3<uint> size = image()->GetGrey()->GetSize();
	if ( _ImgWindowUI)
	{
		// init slider
		uint i = _ImgWindowUI->_winImgView->getId();
		double val = double(size[_imageAxes[i]]);
		_ImgWindowUI->_sliceScrollbar->range( val, 1.0);			// reverse range
		_ImgWindowUI->_sliceScrollbar->linesize( 1.0);
		setWinSlice( val/2);
	}
}

//
// ImgWindow only function.  Called from sliceChange(), initWinSlider().
//
void
Aseg::setWinSlice( double val)
{
	_ImgWindowUI->_sliceScrollbar->value(_ImgWindowUI->_sliceScrollbar->minimum() - val);
}

//
//	Called from resetSegmentation(), restartSegmentation(), stopSegmentation(),
//	stepSegmentation() and fltk idle function - SnakeIdleFunction()
//
void
Aseg::snakeUpdate( bool updateMesh)
{
	static uint lastSize = 0;
	if ( _segUpdateDone)
		return;

	// Update the number of elapsed iterations
	segmentationDlog()->setIterations( segImage3D()->GetElapsedSegmentationIterations());
	segmentationDlog()->setSeconds( segTime_.sec());

	// Update the mesh
	if ( updateMesh)
		updateSegMesh();			// don't show progress

	uint meshSize = segMesh()->MeshSize();
	float change = (float(meshSize) - float(lastSize)) / float(meshSize);
	lastSize = meshSize;
	cout << "Aseg::snakeUpdate: mesh size " << meshSize << " change " << change << endl;
	_segUpdateDone = true;

	// Redraw the windows
	redraw();
}

//
//	Called from loadSegmentation()
//
void
Aseg::updateMesh( bool showProg)		// showProg default false
{
	try
	{
		if ( showProg)
		{
			bool showmesh = showMeshToggle();	// need to not display mesh during update
			if ( showmesh)
				_showMeshToggle->value( false);
			progress()->label( "Generating segmentation mesh");
			progress()->show();
			mesh()->GenerateMesh( progress()->GetRedrawCommand());
			progress()->hide();
			if ( showmesh)
				_showMeshToggle->value( true);
		}
		else
			mesh()->GenerateMesh();
	}
	catch( std::bad_alloc &)
	{
		alert("Out of memory error when generating 3D mesh.");
	}

	updateMeshToggles();
}

//
//	Called from segmentationOK(), snakeUpdate() - showProg false, & setTime()
//
void
Aseg::updateSegMesh( bool showProg)		// showProg default false
{
	try
	{
		if ( showProg)
		{
			bool showmesh = showSegmentationToggle();	// don't display mesh during update
			if ( showmesh)
				_showSegmentationToggle->value( false);
			progress()->label( "Generating segmentation mesh");
			progress()->show();
			segMesh()->GenerateMesh( progress()->GetRedrawCommand());
			progress()->hide();
			if ( showmesh)
				_showSegmentationToggle->value( true);
		}
		else
			segMesh()->GenerateMesh();
	}
	catch( std::bad_alloc &)
	{
		alert("Out of memory error when generating 3D mesh.");
	}
}

//
// display/hide appropriate toggles in MeshView
//
void
Aseg::updateMeshToggles()
{
	for ( uint wt=0; wt < 5; wt++)
	{
		if ( mesh()->GetVTKMesh( wt))
		{
			_MeshDisplayButton[wt]->value(true);
			_MeshDisplayButton[wt]->show();
			if ( _MeshWindowUI)
			{
				_MeshWindowUI->_MeshDisplayButton[wt]->value(true);
				_MeshWindowUI->_MeshDisplayButton[wt]->show();
			}
		}
		else
		{
			_MeshDisplayButton[wt]->hide();
			if ( _MeshWindowUI)
				_MeshWindowUI->_MeshDisplayButton[wt]->hide();
		}
	}
}

//
//	Update threshImage with pixels above threshold.
//
void
Aseg::updateFibrosisThreshold()
{
	LabelImageWrapper3D *threshImage = segImage3D()->GetThreshImage();
	GreyType threshold = _fibrosisHistogram->getThreshold();

	cout << "threshold = " << threshold << endl;
	segImage3D()->updateFibrosisThreshold( threshold);

	setLabelImage( threshImage);
	setDisplayType( DT_FIB);
	showFibrosisThreshold();

	_fileMenu->child( SAVE_FIBROSIS)->activate();
}

void
Aseg::generateWallMeshes()
{
	// adjust SegImage3D walls for no ROI
	itk::Index<3u> in = segROI().GetIndex();
	itk::Size<3u> sz = mainROI().GetSize();
	itk::Size<3u> rsz = segROI().GetSize();

	Pt3<float> indxPt( -(float)in[0], float(0), float(0));
	segImage3D()->adjustWallIndex( indxPt);

	vector<float> spacing = mainImage()->PixelSpacing();

	segMesh()->generateWallMeshes( segImage3D()->getWalls(), spacing[2]);

	showMesh();				// calls redraw()

	// adjust back
	indxPt = -indxPt;
	segImage3D()->adjustWallIndex( indxPt);
}

//
//	public for MeshWindowUI, virtual call from _MeshDisplayButtons.
//
void
Aseg::showMesh( uint wt)
{
	meshView()->showMesh( wt, displayMesh(wt));
	if ( _MeshWindowUI)
		_MeshWindowUI->meshView()->showMesh( wt, _MeshWindowUI->displayMesh(wt));
	redraw();
}

void
Aseg::showInfo()
{
	Pt3<float> pt;
	vector<float> vec;
	itk::Index<3u> in = mainROI().GetIndex();
	itk::Size<3u> sz = mainROI().GetSize();
	int slice = getSlice(0);
	string sname;
	string iname = mainImage()->fileName( slice);

	if ( _imageInfo == 0)
		_imageInfo = new ImageInfo();
	_imageInfo->post();

	// shorten name if longer than 24 char
	shortImageName( iname, sname);
	_imageInfo->setName( sname.c_str());

	_imageInfo->setSize( mainImage()->Width(), mainImage()->Height());
	vec = image()->PixelSpacing();
	_imageInfo->setMmPerPixel( vec[0], vec[1], vec[2]);
	vec = mainImage()->ImageRow();
	_imageInfo->setRowDirection( vec[0], vec[1], vec[2]);
	vec = mainImage()->ImageColumn();
	_imageInfo->setColDirection( vec[0], vec[1], vec[2]);
	vec = mainImage()->ImageNormal();
	_imageInfo->setNormal( vec[0], vec[1], vec[2]);
	_imageInfo->setSeriesNum( mainImage()->SeriesNumber());
	_imageInfo->setTriggerTime( mainImage()->TriggerTime());
	pt = mainImage()->slicePos( slice);
	_imageInfo->setPosition( pt.x(), pt.y(), pt.z());
	_imageInfo->setSliceLoc( mainImage()->sliceLoc( getSlice(0)));
	_imageInfo->setROI( in[0], in[1], sz[0], sz[1]);
	_imageInfo->setMinMax( mainImage()->GetGrey()->GetSlicer(0)->GetSliceMin(),
		mainImage()->GetGrey()->GetSlicer(0)->GetSliceMax());
}

void
Aseg::displayMenuHelp()
{
	alert( "Coming soon!");
}

void
Aseg::analysisMenuHelp()
{
	alert( "Coming soon!");
}

void
Aseg::ThresholdFilterApply()
{
	_thresholdFilter->close();
	setThresholdSettings( _thresholdFilter->settings());

	// Use the SNAPImageData to perform preprocessing
	progress()->label( "Applying threshold filter");
	progress()->show();
	segImage3D()->DoInOutPreprocessing( getThresholdSettings(), progress()->GetRedrawCommand());
	progress()->hide();

	_SegParameters->SetSpeedType( SegParameters::REGION_SNAKE);
	setInitShapeImg( 0);
	setSpeedImage2D( 0);						// clear 2D speed so shows 3D speed
	setSpeedImage( segImage3D()->GetSpeed());
	setDisplayType( DT_SPEED);					// show speed image
	activateMenus();
	redraw();
}

void
Aseg::ThresholdFilterCancel()
{
	// Common closing tasks
	_thresholdFilter->close();
	setInitShapeImg( 0);						// clear 2D speed so shows 3D speed
	setSpeedImage2D( 0);						// clear 2D speed so shows 3D speed
	setSpeedImage( segImage3D()->GetSpeed());	// show existing speed image if any
	setDisplayType( DT_MAIN);
	if ( haveSegmentation())
	{
		_showSpeedToggle->value( false);
		setDisplayType( DT_LABLE);
	}
	activateMenus();
	redraw();
}

void
Aseg::EdgeFilterApply()
{
	_edgeFilter->close();
	setEdgeSettings( _edgeFilter->settings());

	// Use the SNAPImageData to perform preprocessing
	setCursor( XC_watch);
	progress()->label( "Applying edge filter");
	progress()->show();
	segImage3D()->DoEdgePreprocessing( getEdgeSettings(), progress()->GetRedrawCommand());
	progress()->hide();
	setCursor( XC_left_ptr);

	_SegParameters->SetSpeedType( SegParameters::EDGE_SNAKE);
	setInitShapeImg( 0);
	setSpeedImage2D( 0);						// clear 2D speed so shows 3D speed
	setSpeedImage( segImage3D()->GetSpeed());
	setDisplayType( DT_SPEED);					// show speed image
	activateMenus();
	redraw();
}

void
Aseg::EdgeFilterCancel()
{
	// Common closing tasks
	_edgeFilter->close();
	setInitShapeImg( 0);
	setSpeedImage2D( 0);						// clear 2D speed so shows 3D speed
	setSpeedImage( segImage3D()->GetSpeed());	// show existing speed image if any
	setDisplayType( DT_MAIN);
	if ( haveSegmentation())
	{
		_showSpeedToggle->value( false);
		setDisplayType( DT_LABLE);
	}
	activateMenus();
	redraw();
}

void
Aseg::setBubblesOk()
{
	setStatus( "Bubbles set, next set parameters");
	activateMenus();
}

void
Aseg::shortImageName( string &iname, string &sname)
{
	int len;
	if ( ( len = iname.length()) > 36)
	{
		string firstPart, middlePart, lastPart;
		firstPart = iname.substr(0, 16);
		middlePart = "...";
		lastPart = iname.substr( iname.length()-16);
		sname = firstPart + middlePart + lastPart;
	}
	else
		sname = iname;
}

void
Aseg::colorOK()
{
	cout << "Color: " << _ColorLabelDlog->getLabelColor() << endl;
	SetDrawingColorLabel( _ColorLabelDlog->getLabelColor());
//	segImage3D()->SetSnakeColorLabel( _ColorLabelDlog->getLabelColor());
//	meshView()->setViewMode( MeshView::VM_SET_COLOR);
//	setStatus( "Select wall to color");
}

//*****************************************************************************
//
//	Callback functions
//
//*****************************************************************************

//
//	ThresholdFilter callbacks
//
void
Aseg::ThresholdFilterOkCB(	Widget* o, void* callData)
{
	((Aseg *)callData)->ThresholdFilterApply();
}

void
Aseg::ThresholdFilterApplyCB(	Widget* o, void* callData)
{
	((Aseg *)callData)->ThresholdFilterApply();
}

void
Aseg::ThresholdFilterCancelCB(	Widget* o, void* callData)
{
	((Aseg *)callData)->ThresholdFilterCancel();
}

//
//	EdgeFilter callbacks
//
void
Aseg::EdgeFilterOkCB(	Widget* o, void* callData)
{
	((Aseg *)callData)->EdgeFilterApply();
}

void
Aseg::EdgeFilterApplyCB(	Widget* o, void* callData)
{
	((Aseg *)callData)->EdgeFilterApply();
}

void
Aseg::EdgeFilterCancelCB(	Widget* o, void* callData)
{
	((Aseg *)callData)->EdgeFilterCancel();
}

void
Aseg::setBubblesOkCB( Widget* o, void* callData)
{
	((Aseg *)callData)->setBubblesOk();
}

void
Aseg::setBubblesCancelCB( Widget* o, void* callData)
{
	Aseg *obj = (Aseg *)callData;
	obj->setViewMode( ImgView::VM_CROSS_HAIR);
	obj->redraw();
}

void
Aseg::editParametersOkCB( Widget* o, void* callData)
{
	Aseg *obj = (Aseg *)callData;
	obj->_SegParameters->checkParameters();
	obj->_SegParameters->setAlgorithm();		// init's class params
	obj->setStatus( "Ready to segment");
	obj->activateMenus();
}

void
Aseg::editParametersCancel( Widget* o, void* callData)
{
	((Aseg *)callData)->setStatus( "Using current parameters");
}

void
Aseg::SegmentationOkCB( Widget* o, void* callData)
{
	((Aseg *)callData)->segmentationOK();
}

void
Aseg::SegmentationCancelCB( Widget* o, void* callData)
{
	((Aseg *)callData)->segmentationCancel();
}

void
Aseg::AddWallOkCB( Widget* o, void* callData)
{
	Aseg *obj = (Aseg *)callData;
	obj->segImage3D()->closeWall();
//	obj->mesh()->generateWallMeshes( obj->segImage3D()->getWalls());
	obj->setViewMode( ImgView::VM_CROSS_HAIR);
	obj->activateMenus();
	obj->redraw();
}

void
Aseg::AddWallCancelCB( Widget* o, void* callData)
{
	Aseg *obj = (Aseg *)callData;
	obj->setViewMode( ImgView::VM_CROSS_HAIR);
	obj->segImage3D()->deleteWall();
	obj->redraw();
}

void
Aseg::FibrosisOkCB( Widget* o, void* callData)
{
	FibrosisApplyCB( o, callData);
}

void
Aseg::FibrosisApplyCB( Widget* o, void* callData)
{
	((Aseg *)callData)->updateFibrosisThreshold();
}

void
Aseg::intensityOkCB( fltk::Widget* o, void* callData)
{
	((Aseg *)callData)->intensityOk();
}

void
Aseg::intensityCancelCB( fltk::Widget* o, void* callData)
{
	((Aseg *)callData)->intensityCancel();
}

void
Aseg::colorOkCB( fltk::Widget* o, void* callData)
{
	((Aseg *)callData)->colorOK();
}
