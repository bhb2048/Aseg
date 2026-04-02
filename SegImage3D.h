//*****************************************************************************
//	SegImage3D.h -  class for segmentation image
//
//	29-dec-11  bhb	subclass of ItkImage3D, following snap/Logic/Framework/SNAPImageData
//	Modified:
//	06-jan-12  bhb	make m_CurrentSnakeParameters a pointer
//	25-jan-12  bhb	add m_LabelWrapper & segmentaton functions from ItkImage3D
//	26-jan-12  bhb	add InitializeLabel()
//	02-feb-12  bhb	changed name from AsegImage to SegImage
//	27-mar-12  bhb	add m_Walls, m_CurWall
//	11-apr-12  bhb	add m_ThreshLabelWrapper
//	04-may-12  bhb	add IsThreshLoaded()
//	05-jun-12  bhb	add insertWallPt(), deleteWallPt(), hasCurWalls(), 
//						getSegmentationVolume(), getPolgon()
//	19-jul-12  bhb	add getMyoPixels(), setSlice(), addVoxels(), checkSingleIsects(), 
//					updateFibrosisThreshold()
//	21-jul-12  bhb	Add SegImage3D::adjustWallIndex()
//	02-aug-12  bhb	need >1 WallType in a slice, add WallSet
//	08-aug-12  bhb	add m_WallColors
//	22-aug-12  bhb	add hasWall( uint wt)
//	01-nov-12  bhb	add clearSegmentation()
//	03-dec-13  bhb	add Set/GetSnakeColorLabel(), setLabelColor()
//	13-oct-14  bhb	add FillBubbles()
//	17-nov-14  bhb	add InitLevelsetFromMesh() change FillBubbles() name to 
//						InitLevelsetFromBubbles()
//	23-mar-15  bhb	change ACWE segmentation to GeodesicActiveContour segmentation
//	07-apr-15  bhb	renamed m_PreprocessingDone to m_InOutPreprocessingDone, added
//					m_EdgePreprocessingDone
//	10-jun-15  bhb	remove InitLevelsetFromMesh(), don't need
//	01-oct-15  bhb	add InitializeLevelSet(), GetHistogram()
//	07-oct-15  bhb	For 2D seg: add 2d wrappers, segment2D(), GetSnake2D(), LevelSetDriver2D(),
//					GetElapsedSegmentationIterations2D(), GetLevelSetImage2D(),
//					GetLevelSetFunction2D(), m_SnakeWrapper2D
//	13-oct-15  bhb	add IsSnakeLoaded2D()
//	28-oct-15  bhb	add m_SpeedWrapper2D
//	12-nov-15  bhb	remove GeodesicActiveContour()
//	26-jan-16  bhb	remove GetElapsedSegmentationIterations2D(), combine in 
//					GetElapsedSegmentationIterations()
//	14-jun-17  bhb	modify to use SegParameters instead of renamed SnakeParameters
//	15-jun-17  bhb	add InitializeSeeds()
//	12-jul-17  bhb	remove 2D segmentation operation
//	13-sep-17  bhb  add m_ShapeWrapper, getInitialShape()
//	21-sep-17  bhb	add isShapeLoaded()
//	
//*****************************************************************************
#ifndef __ITKSEGIMAGE3D_H
#define __ITKSEGIMAGE3D_H

#include <fltk/gl.h>					// need for _HAVE_GL
#include <ITK/ItkImage3D.h>
#include <ITK/LabelImageWrapper3D.h>
#include <ITK/SpeedImageWrapper.h>
#include <ITK/SpeedImageWrapper3D.h>
#include <ITK/LevelSetImageWrapper3D.h>
#include <ITK/EdgePreprocessingSettings.h>
#include <ITK/ThresholdSettings.h>
#include <ITK/ImageCoordinateTransform.h>
#include <ITK/ImageCoordinateGeometry.h>
#include "LevelSet/SegLevelSetDriver.h"
#include <Geom/Sphere.h>
#include <Geom/SplineCurve.h>
#include "itkFastMarchingImageFilter.h"

#include "itkOpenCLUtil.h"
#include "itkGPUImage.h"
#include "itkGPUKernelManager.h"

#include <vector>
#include <string>
#include <fstream>

class MeshObject;

class SegImage3D : public ItkImage3D
{
	public:
		enum	WallType { NOWALL=-1, EPI, LAENDO, RAENDO, LVENDO, RVENDO };
		enum	{ Dimension = 3 };

		struct WallSet
		{
			WallSet() { for ( uint i=0; i<5; i++) 
						wall[i].setType( SplineCurve< Pt3<float>, float >::CATMULL_ROM);	}
			bool hasWall()	{	for ( uint i=0; i<5; i++)
									if ( wall[i].size()) return true;
								return false;
							}
			SplineCurve< Pt3<float>, float >	wall[5];
		};
		
		// The type of the internal level set image
		typedef LabelImageWrapper3D::ImageType			LabelImageType;
		typedef itk::Image<float,Dimension>				FloatImageType;
		typedef itk::Image<float,2>						FloatImageType2D;
		typedef FloatImageType							LevelSetImageType;
		typedef itk::Image< LabelTypeUS,2>				LabelImage2dType;
		typedef itk::Index<Dimension>					IndexType;
		typedef itk::FastMarchingImageFilter< FloatImageType, FloatImageType>
														FastMarchingFilterType;
		typedef FastMarchingFilterType::NodeContainer	NodeContainer;
		// 2017-07-05 can't use NodeContainer var as class member, get ITK SEGV's
		typedef FastMarchingFilterType::NodeType		NodeType;

		SegImage3D();
		~SegImage3D();

		void	SetGreyImage( GreyImageType *newGreyImage,
								const ImageCoordinateGeometry &newGeometry,
								const GreyTypeToNativeFunctor &native);

		// SpeedWrapper functions
		bool IsSpeedLoaded()	{ return m_SpeedWrapper.IsInitialized();	}
		bool IsSpeedLoaded2D()	{ return m_SpeedWrapper2D.IsInitialized();	}
		void InitializeSpeed2D( FloatImageType2D *img) { m_SpeedWrapper2D.SetImage( img);	}
		void InitializeSpeed()	{ m_SpeedWrapper.InitializeToWrapper( &m_GreyWrapper, 0.F);	}
		SpeedImageWrapper3D *GetSpeed()	{ return m_SpeedWrapper.IsInitialized() ?
												&m_SpeedWrapper : 0;	}
		SpeedImageWrapper *GetSpeed2D()	{ return m_SpeedWrapper2D.IsInitialized() ?
												&m_SpeedWrapper2D : 0;	}

		bool IsSnakeLoaded()				{ return m_SnakeWrapper.IsInitialized();	}
		LevelSetImageWrapper3D*	GetSnake()	{ return m_SnakeWrapper.IsInitialized() ?
												&m_SnakeWrapper : 0;	}
		bool isShapeLoaded()				{ return m_ShapeWrapper.IsInitialized();	}
		LabelImageWrapper3D*	GetShape()	{ return m_ShapeWrapper.IsInitialized() ? 
												&m_ShapeWrapper : 0;	}

		SegLevelSetDriver<3> * LevelSetDriver()		{ return m_LevelSetDriver;	}

		SetMacro(SnakeColorLabel, LabelTypeUS);
		GetMacro(SnakeColorLabel, LabelTypeUS);

		SetMacro(ColorLabel, ColorLabel);
		GetMacro(ColorLabel, ColorLabel);

		// SNAPImageData functions
		void	DoEdgePreprocessing(const EdgePreprocessingSettings &settings,
									itk::Command *progressCB=0);
		void	DoInOutPreprocessing(const ThresholdSettings &settings,
									itk::Command *progressCB=0);

		// Segmentation image functions
		
		//	Access the segmentation image (read only access allowed to preserve state)
		LabelImageWrapper3D* GetSegmentation()
		{ return m_LabelWrapper.IsInitialized() ? &m_LabelWrapper : 0;	}

		//	This method sets the segmentation image (see note for SetGrey).
		bool 	SetSegmentationImage( LabelImageType *newLabelImage);

		void 	clearSegmentation()
				{ m_LabelWrapper.InitializeToWrapper( &m_GreyWrapper, (LabelTypeUS) 0);	}

		void	SetColorLabelTable( ColorLabelTable	*ct)
				{ if (  m_LabelWrapper.IsInitialized())
					m_LabelWrapper.SetLabelColorTable(ct);	}

		//	Set voxel in segmentation image
		void	SetSegmentationVoxel(const Vec3<uint> &index, LabelTypeUS value);

		//	Check validity of segmentation image
		bool	IsSegmentationLoaded()	{ return m_LabelWrapper.IsInitialized();	}
		
		bool	InitializeSegmentation( const SegParameters *parameters, 
					const std::vector<Sphere<uint> > &bubbles, 
					LabelTypeUS labelColor, itk::Command *progressCB = 0);
		bool	checkPCAFiles();
		bool	getInitialShape();
		void	InitializeLevelSet( unsigned long &nInitVoxels);
		void	InitLevelsetFromBubbles( const std::vector<Sphere<uint> > &bubbles,
					unsigned long &nInitVoxels);
		bool	InitializeFastMarchingFromBubbles( const std::vector<Sphere<uint> > &bubbles);
		void	InitalizeSnakeDriver( const SegParameters *p, itk::Command *progressCB = 0);
		void	RunSegmentation( unsigned int nIterations);
		void	RestartSegmentation();
		void	TerminateSegmentation();
		uint	GetElapsedSegmentationIterations() const
				{ return m_LevelSetDriver->GetElapsedIterations();	}
		LevelSetImageType *	GetLevelSetImage()	{ return m_LevelSetDriver ? 
					m_LevelSetDriver->GetCurrentState() : 0;}
		SegLevelSetFunction<SpeedImageWrapper3D::ImageType> *GetLevelSetFunction()
				{ return m_LevelSetDriver->GetLevelSetFunction();	}
		bool		InOutPreprocessingDone()		{ return m_InOutPreprocessingDone;	}
		bool		EdgePreprocessingDone()		{ return m_EdgePreprocessingDone;	}
		bool		SegmentatonDone()		{ return m_SegmentatonDone;	}

		//	wall functions - used for fibrosis currently
		void		addWall( uint wt);
		void		addWallPt( Pt3<float> &pt);
		void		insertWallPt( Pt3<float> &pt);
		void		deleteWallPt( Pt3<float> &pt);
		void		closeWall();
		Pt3<float> *closestPoint( Pt3<float> &pt);
		int			closestEdge( Pt3<float> &pt);
		WallType	curWall()		{ return m_CurWall; }
		void		curWall( uint wt)	{ m_CurWall = WallType(wt);	}
		std::vector< std::vector<WallSet> > *	getWalls()	{ return &m_Walls;	}
		void		deleteWalls();
		void		deleteWall()	{ if ( wallSelected() && m_CurWallSet >=0) 
								m_Walls[getSlice()][m_CurWallSet].wall[m_CurWall].clear();
								m_EditWall = false;	}
		void		drawWalls();
		void		editWall( bool v)	{ m_EditWall = v;	}
		bool		hasWalls();
		bool		hasWall( uint wt);
		bool		hasCurWalls();
		bool		hasPrevWalls();
		bool		hasNextWalls();
		bool		wallSelected()		{ return m_CurWall > NOWALL;	}
		void		loadWalls( std::ifstream &ifstrm, Pt3<float> &indxPt);
		void		saveWalls( std::ofstream &ofstrm, Pt3<float> &indxPt);
		void		adjustWallIndex( Pt3<float> &indxPt);
		void		copyWalls( uint n);
		void		setCurWallChanged();
		uint		getSlice();
		void		setSlice( uint slice);
		float		getSegVolume()			{ return m_Volume[0];	}
		float		getWallVolume( uint w)	{ return m_Volume[w];	}
		
		// threshold image functions (fibrosis)
		void		SetThreshImage( const ImageCoordinateGeometry &geometry);
		LabelImageWrapper3D* GetThreshImage()
		{ return m_ThreshLabelWrapper.IsInitialized() ? &m_ThreshLabelWrapper : 0;	}
		
		bool 		IsThreshLoaded()	{ return m_ThreshLabelWrapper.IsInitialized();	}
		bool		getSegmentationVolume( char *errMsg);
		bool		getPolgon( const LabelImage2dType *image, LabelTypeUS val,
									Polygon<Pt2<float>, float> &poly);
		bool		getMyoPixels( uint wt, uint viewId, std::vector<GreyType> &pixels);
		void		addVoxels( uint viewId, Pt3<float> &pt1, Pt3<float> &pt2, 
								std::vector<GreyType> &pixels);
		bool		checkSingleIsects( std::vector<Pt3<float> > &ipts1, 
										std::vector<Pt3<float> > &ipts2, bool epi);
		void		updateFibrosisThreshold( GreyType threshold);
		void		saveFibrosis( std::ofstream &ofstrm, GreyType minPixel, GreyType maxPixel,
						GreyType threshold, uint nAbove, float perCentAbove);
		void		setLabelColor( Pt3<float> &pos);

	protected:
		WallType	whichWall( ifstream &ifstrm);
		unsigned long countVoxelsColor( LabelImageWrapper3D::ImagePointer img, 
										LabelTypeUS color);

	private:
		// Wrapper around the segmentation image
		LabelImageWrapper3D			m_LabelWrapper;
		
		// Wrapper for the intensity image
		SpeedImageWrapper3D			m_SpeedWrapper;
		SpeedImageWrapper			m_SpeedWrapper2D;
		
		// Wrapper around the initial shape image
		LabelImageWrapper3D			m_ShapeWrapper;

		// Snake initialization data (initial distance transform)
		LevelSetImageWrapper3D		m_SnakeInitializationWrapper;

		// Wrapper around the level set image
		LevelSetImageWrapper3D		m_SnakeWrapper;

		// Wrapper around the threshold (fibrosis) image
		LabelImageWrapper3D			m_ThreshLabelWrapper;
		
		// Snake driver
		SegLevelSetDriver<3> *		m_LevelSetDriver;

		// Label color used for the snake images
		LabelTypeUS					m_SnakeColorLabel;

		// Current value of snake parameters
		const SegParameters *		m_SegParameters;       

		// Typedefs for defining the advection image that can be loaded externally
		typedef itk::FixedArray<float, 3> VectorType;
		typedef itk::Image< VectorType, 3> VectorImageType;
		typedef itk::SmartPointer<VectorImageType> VectorImagePointer;

		// The advection image
		VectorImagePointer			m_ExternalAdvectionField;

		// The color label that is used for this segmentation
		ColorLabel					m_ColorLabel;

		bool						m_InOutPreprocessingDone;
		bool						m_EdgePreprocessingDone;
		bool						m_SegmentatonDone;

		std::vector< std::vector<WallSet> >	m_Walls;	// WallSet for each slice
		WallType					m_CurWall;
		int							m_CurWallSet;		// index of current WallSet
		bool						m_EditWall;
		float						m_Volume[4];
		std::vector< std::vector<GreyType> >	_myoPixels[4];	// each endo wall, each slice
		std::vector<Vec3<uint> >	_myoPoints;			// for Fibrosis threshold calc
		float						m_WallColors[5][4];
		template<typename ImgType> void	GetMinMax( ImgType *img, 
			typename ImgType::PixelType &min, typename ImgType::PixelType &max, 
			const char *msg=0);
		template<typename ImgType> void	GetHistogram( ImgType *img, const char *msg);
		template<typename InImgType, typename OutImgType> typename OutImgType::Pointer 
			rescaleImage( InImgType *inImg, typename OutImgType::PixelType outMin, 
										typename OutImgType::PixelType outMax = 0);
};
#endif
