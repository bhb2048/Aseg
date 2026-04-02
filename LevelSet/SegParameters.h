/*=========================================================================

  Program:   ITK-SNAP
  Module:	$RCSfile: SnakeParameters.h,v $
  Language:  C++
  Date:	  $Date: 2007/12/30 04:05:15 $
  Version:   $Revision: 1.2 $
  Copyright (c) 2007 Paul A. Yushkevich

  This file is part of ITK-SNAP

  ITK-SNAP is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  -----

  Copyright (c) 2003 Insight Software Consortium. All rights reserved.
  See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
//	23-dec-11  bhb  from ItkSnap/Logic/LevelSet/SnakeParameters.h
//	Modified:
//	26-dec-11  bhb  made subclass of SnakeParametersUI
//	17-sep-15  bhb	add GEO_ACT_CONT SolverType
//	22-sep-15  bhb	add setRMSError()
//	24-sep-15  bhb	add setAdvection(), changed SnakeType enum/var to SpeedType
//	23-feb-16  bhb	add checkParameters()
//	13-jun-17  bhb	renamed from SnakeParameters to SegParameters, using new UI from Sticks
//	14-jun-17  bhb	remove SetDefaultEdgeParameters(), SetDefaultInOutParameters(), 
//					SetDefaultAllZeroParameters() - unused
//	06-jul-17  bhb	add additional params for GeoActContShape, including seed radius
//	07-sep-17  bhb	add ShapeStartZ
//	
#ifndef __SegParameters_h_
#define __SegParameters_h_

#include <ITK/Common.h>
#include "../UI/SegParametersUI.h"
#include <string>
namespace fltk
{
	class FileChooser;
}
/**
 * class SegParameters
 * Parameters for the Level Set evolution.
 * Most of these parameters correspond to the terms in LevelSetFunction.
 *
 * itk::LevelSetFunction
 */
class SegParameters : public SegParametersUI
{
	public:
		SegParameters();
		virtual ~SegParameters();

		enum SpeedType { EDGE_SNAKE, REGION_SNAKE	};
		enum SolverType { PARALLEL_SPARSE_FIELD_SOLVER, DENSE_SOLVER, GEO_ACT_CONT, 
			GEO_ACT_CONT_SHAPE};

		void	setDirectory( std::string &d)		{ _directory = d;	}
		void	setGeodMean( const char *name)		{ _GeodShapeMeanInput->value( name);}
		void	setGeodModeFmt( const char *name)	{ _GeodShapeModeFmt->value( name);	}
		
		void	checkParameters();
		
		// Define a comparison operator
		bool operator ==(const SegParameters &p) const;

		//	Whether we wish to automatically compute optimal time step
		//	in level snake propagation
		GetMacro(AutomaticTimeStep,bool);
		SetMacro(AutomaticTimeStep,bool);

		//	Time step factor in level snake propagation.  This is is only used
		//	if the automatic computation is off, and represents the factor by
		//	which the auto time step is multiplied
		GetMacro(TimeStepFactor,float);
		SetMacro(TimeStepFactor,float);

		//	Which solver to use to run the equation
		GetMacro(Solver,SolverType);
		SetMacro(Solver,SolverType);

		//	Type of equation (well known parameter sets)
		GetMacro(SpeedType,SpeedType);
		SetMacro(SpeedType,SpeedType);

		GetMacro(AdvectionWeight,float);
		SetMacro(AdvectionWeight,float);

		GetMacro(AdvectionSpeedExponent,int);
		SetMacro(AdvectionSpeedExponent,int);

		GetMacro(PropagationWeight,float);
		SetMacro(PropagationWeight,float);

		GetMacro(PropagationSpeedExponent,int);
		SetMacro(PropagationSpeedExponent,int);

		GetMacro(CurvatureWeight,float);
		SetMacro(CurvatureWeight,float);

		GetMacro(RMSError,float);
		SetMacro(RMSError,float);

		GetMacro(CurvatureSpeedExponent,int);
		SetMacro(CurvatureSpeedExponent,int);

		GetMacro(LaplacianWeight,float);
		SetMacro(LaplacianWeight,float);

		GetMacro(LaplacianSpeedExponent,int);
		SetMacro(LaplacianSpeedExponent,int);

		GetMacro(ShapeStartX, int);
		SetMacro(ShapeStartX, int);
		GetMacro(ShapeStartY, int);
		SetMacro(ShapeStartY, int);
		GetMacro(ShapeStartZ, int);
		SetMacro(ShapeStartZ, int);
		GetMacro(NumPCAModes, int);
		SetMacro(NumPCAModes, int);

		// cost function weights
		GetMacro(ContourWt, double);
		SetMacro(ContourWt, double);
		GetMacro(ImageWt, double);
		SetMacro(ImageWt, double);
		GetMacro(ShapePriorWt, double);
		SetMacro(ShapePriorWt, double);
		GetMacro(PosePriorWt, double);
		SetMacro(PosePriorWt, double);

		GetMacro(ShapeScaling, float);
		SetMacro(ShapeScaling, float);

		GetMacro(ShapeMeanInput, char *);
		SetMacro(ShapeMeanInput, char *);
		GetMacro(ShapeModeFormat, char *);
		SetMacro(ShapeModeFormat, char *);

		GetMacro(SeedRadius, int);
		SetMacro(SeedRadius, int);
		
		virtual void setAlgorithm();

	private:

		float	m_TimeStepFactor;

		SpeedType m_SpeedType;

		bool	m_AutomaticTimeStep;

		float	m_AdvectionWeight;
		int		m_AdvectionSpeedExponent;

		float	m_PropagationWeight;
		int		m_PropagationSpeedExponent;

		float	m_CurvatureWeight;
		int		m_CurvatureSpeedExponent;
		
		float	m_LaplacianWeight;
		int		m_LaplacianSpeedExponent;

		float	m_RMSError;

		int		m_ShapeStartX;
		int		m_ShapeStartY;
		int		m_ShapeStartZ;
		int		m_NumPCAModes;

		// cost function weights
		double	m_ContourWt;
		double	m_ImageWt;
		double	m_ShapePriorWt;
		double	m_PosePriorWt;

		float	m_ShapeScaling;

		char *	m_ShapeMeanInput;
		char *	m_ShapeModeFormat;

		int		m_SeedRadius;			// not from UI
		
		SolverType m_Solver;

		fltk::FileChooser *	_fileChooser;
		std::string		 _directory;

		virtual void loadSegParameters();
		virtual void saveSegParameters();

};

#endif // __SegParameters_h_
