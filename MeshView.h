//
//	MeshView.h - class for viewing 3D ITK mesh object
//	
//	29-dec-11  bhb
//	Modified:
//	31-jul-13  bhb	add ViewMode enum, _viewMode, setViewMode() for Pic3Model registration
//					add rotateMesh()
//	23-oct-13  bhb	add bbox()
//	03-jun-14  bhb	add displayImg()
//	09-jun-14  bhb	add _modZoom
//	10-apr-15  bhb	add _mesh, _segMesh
//	16-apr-15  bhb	add _regMesh
//	23-feb-16  bhb	make _modTransMatrix, _modRotMatrix arrays, add #define MAX_MESH_WALLS
//	24-feb-16  bhb	add _startX[], _startY[] for mesh registration
//	
#ifndef _MeshView_H
#define _MeshView_H

#include <PlotFl/ModelView.h>
#include <fltk/gl.h>
#include <Math/Algebra.h>
#include <Geom/TransMatrix.h>
#include <Plot/OpenGLTexture.h>
#include <ITK/VectorTypes.h>
#include <vnl/vnl_matrix_fixed.h>

class Aseg;
class GreyImageWrapper3D;
class MeshObject;
class GlFont;

#define MAX_MESH_WALLS	5

class MeshView : public ModelView
{
	public:

		enum ViewMode	{ VM_NONE, VM_REGISTER, VM_SET_COLOR };
		
		MeshView( int x, int y, int w, int h);
		~MeshView();

		// access
		Aseg *					parent()					{ return _parent;	}
		MeshObject *			mesh()						{ return _mesh;}
		void					regMesh( MeshObject *m)		{ _regMesh = m;	}
		MeshObject *			regMesh()					{ return _regMesh;}
		void					segMesh( MeshObject *m)		{ _segMesh = m;	}
		MeshObject *			segMesh()					{ return _segMesh;}
		
		void					initialize( Aseg *p, MeshObject *mesh);
		void					setImage( GreyImageWrapper3D *img)	{_image = img;	}
		GreyImageWrapper3D *	image()						{ return _image;	}
		void					showMesh( uint wt, bool val)	{ _showMesh[wt] = val;	}
		bool					showMesh( uint wt)			{ return _showMesh[wt];	}
		Bbox<Pt3<float>, float> *bbox()						{ return &_Bbox;	}
		void					setViewMode( ViewMode vm)	{ _viewMode = vm;	}
		TransMatrix *			modTransMatrix( uint i)		{ return &_modTransMatrix[i]; }
		TransMatrix *			modRotMatrix( uint i)		{ return &_modRotMatrix[i]; }
		void					resetRegistration();
		float					modZoom()					{ return _modZoom;	}
		OpenGLTexture &			imgTex( uint i)				{ return _imgTex[i];  }
		
	private:	
		Aseg *					_parent;			// parent app
		bool					_init;
		GreyImageWrapper3D *	_image;					// our image
		MeshObject *			_mesh;
		MeshObject *			_regMesh;
		MeshObject *			_segMesh;
		Vec3<uint>				_imageSize;
		Vec3<double>			_pixelSpacing;
		uint					_imageAxes[3];
		Bbox<Pt3<float>, float>	_Bbox;
		Pt3<float> 				_minPt, _maxPt;
		ViewMode				_viewMode;
		bool					_showMesh[MAX_MESH_WALLS];
		TransMatrix     		_modTransMatrix[MAX_MESH_WALLS];
		TransMatrix     		_modRotMatrix[MAX_MESH_WALLS];
		float					_startX[MAX_MESH_WALLS];
		float					_startY[MAX_MESH_WALLS];
		float					_modZoom;
		OpenGLTexture 			_imgTex[3];				// current texture image
		Vec3<uint> 				_lastSliceIndx;
		
		// Matrix from voxel space to world coordinates (NIFTI/RAS coords)
		typedef vnl_matrix_fixed<double, 4, 4> Mat4d;
		Mat4d					m_WorldMatrix;

		// ModelView overloads
		virtual void	initGraphic();
		virtual void	draw();
		virtual int		handle( int event);
		virtual void	drawModel( GLenum rendMode = GL_RENDER);
		
		void			displayImg();

		void			drawCube();
		void			rotateMesh();

		static void 	rotateMeshTimerCB( void* v);
};
#endif
