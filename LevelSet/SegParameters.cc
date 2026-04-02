/*=========================================================================

	Program:	ITK-SNAP
	Module:	$RCSfile: SnakeParameters.cxx,v $
	Language:	C++
	Date:		$Date: 2010/10/19 19:17:00 $
	Version:	$Revision: 1.3 $
	Copyright (c) 2007 Paul A. Yushkevich

	This file is part of ITK-SNAP

	ITK-SNAP is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.	If not, see <http://www.gnu.org/licenses/>.

	-----

	Copyright (c) 2003 Insight Software Consortium. All rights reserved.
	See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

	This software is distributed WITHOUT ANY WARRANTY; without even
	the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
	PURPOSE.	See the above copyright notices for more information.
=========================================================================*/
//
//	Modified:
//	28-dec-11 bhb	init DefaultInOutParameters in ctor, change 'Get' funcs to 'Set'
//	29-sep-15  bhb	Update slider, choice vals in loadSnakeParameters()
//	13-jun-17  bhb	renamed from SnakeParameters to SegParameters, using new UI from Sticks
//	14-jun-17  bhb	remove SetDefaultEdgeParameters(), SetDefaultInOutParameters(), 
//					SetDefaultAllZeroParameters() - unused
//	
#include "SegParameters.h"
#include <fltk/FileChooser.h>
#include <fltk/run.h>
#include <fltk/ask.h>
#include <iostream>
#include <fstream>

using namespace std;
using namespace fltk;

SegParameters::SegParameters() : SegParametersUI(), _fileChooser(0)
{
	// set DefaultInOutParameters
	m_AutomaticTimeStep = true;
	m_TimeStepFactor = 1.0F;

	m_SpeedType = REGION_SNAKE;

	m_PropagationWeight = 1.0;
	m_PropagationSpeedExponent = 1;

	m_CurvatureWeight = 0.2;
	m_CurvatureSpeedExponent = -1;

	m_LaplacianWeight = 0.0F;
	m_LaplacianSpeedExponent = 0;

	m_AdvectionWeight = 0;
	m_AdvectionSpeedExponent = 0;

	m_Solver = PARALLEL_SPARSE_FIELD_SOLVER;

	_directory = getenv( "IMAGE_DIR");
}

SegParameters::~SegParameters()
{
	
}

//
//	Called from Aseg::editParametersOkCB()
//	
void
SegParameters::checkParameters()
{
	if ( GetSpeedType() == REGION_SNAKE && m_AdvectionWeight != 0.F)
	{
		alert( "For threshold (in/out) segmentation, advection must equal 0, setting to 0.");
		_PSF_Advection->value(0);
		_DFD_Advection->value(0);
		_GeodAdvection->value(0);
		m_AdvectionWeight = 0.F;
	}
}

bool
SegParameters::operator == (const SegParameters &p) const
{
	return(
	m_AutomaticTimeStep == p.m_AutomaticTimeStep &&
	(m_AutomaticTimeStep || (m_TimeStepFactor == p.m_TimeStepFactor)) &&
	m_SpeedType == p.m_SpeedType &&
	m_PropagationWeight == p.m_PropagationWeight &&
	m_PropagationSpeedExponent == p.m_PropagationSpeedExponent &&
	m_CurvatureWeight == p.m_CurvatureWeight &&
	m_CurvatureSpeedExponent == p.m_CurvatureSpeedExponent &&
	m_LaplacianWeight == p.m_LaplacianWeight &&
	m_LaplacianSpeedExponent == p.m_LaplacianSpeedExponent &&
	m_AdvectionWeight == p.m_AdvectionWeight &&
	m_AdvectionSpeedExponent == p.m_AdvectionSpeedExponent &&
	m_Solver == p.m_Solver);
}

//
//	virtual UI functions
//	
void
SegParameters::setAlgorithm()
{
	switch ( _SegmentTabGroup->value())
	{
		case 0:
		default:
			m_Solver = PARALLEL_SPARSE_FIELD_SOLVER;
			m_AdvectionWeight = _PSF_Advection->value();
			m_PropagationWeight = _PSF_Propagation->value();
			m_CurvatureWeight = _PSF_Curvature->value();
			m_RMSError = _PSF_RMS_Error->value();
			break;
		case 1:
			m_Solver = DENSE_SOLVER;
			m_AdvectionWeight = _DFD_Advection->value();
			m_PropagationWeight = _DFD_Propagation->value();
			m_CurvatureWeight = _DFD_Curvature->value();
			m_RMSError = _DFD_RMS_Error->value();
			break;
		case 2:
			m_Solver = GEO_ACT_CONT;
			m_AdvectionWeight = _GeodAdvection->value();
			m_PropagationWeight = _GeodPropagation->value();
			m_CurvatureWeight = _GeodCurvature->value();
			m_RMSError = _GeodRMSError->value();
			break;
		case 3:
			m_Solver = GEO_ACT_CONT_SHAPE;
			m_AdvectionWeight = _GeodShapeAdvection->value();
			m_PropagationWeight = _GeodShapePropagation->value();
			m_CurvatureWeight = _GeodShapeCurvature->value();
			m_RMSError = _GeodShapeRMSError->value();
			m_ShapeStartX = _GeodShapeX->value();
			m_ShapeStartY = _GeodShapeY->value();
			m_ShapeStartZ = _GeodShapeZ->value();
			m_NumPCAModes = _GeodShapeNumPCAModes->value();

			m_ContourWt = _GeodShapeContourWt->value();
			m_ImageWt = _GeodShapeImageWt->value();
			m_ShapePriorWt = _GeodShapePriorWt->value();
			m_PosePriorWt = _GeodShapePoseWt->value();

			m_ShapeScaling = _GeodShapeScaling->value();
			m_ShapeMeanInput = const_cast<char *>(_GeodShapeMeanInput->value());
			m_ShapeModeFormat = const_cast<char *>(_GeodShapeModeFmt->value());
			break;
	}
}

void
SegParameters::loadSegParameters()
{
	if ( _fileChooser == 0)
	{
		_fileChooser = new FileChooser( _directory.c_str(), "*.par",
					FileChooser::SINGLE, "Load Segmentation Parameters");
	}
	else
	{
		_fileChooser->type( FileChooser::SINGLE);
		_fileChooser->label("Load Segmentation Parameters");
		_fileChooser->filter( "*.par");
	}
	_fileChooser->show( 300, 300);

	while ( _fileChooser->visible())
		fltk::wait();

	if ( _fileChooser->count() > 0)
	{
		ifstream inFile;
		inFile.open( _fileChooser->value(), ios::in);
		if ( inFile)
		{
			int type;
			string line;
			getline( inFile, line);		// should be 'Snake Parameters'
			inFile >> m_TimeStepFactor;
			inFile >> type;
//			m_SpeedType = SpeedType(type);	// 30-sep-15, don't change SpeedType, set elsewhere
			inFile >> m_AutomaticTimeStep;
			inFile >> m_PropagationWeight;
			inFile >> m_PropagationSpeedExponent;
			inFile >> m_CurvatureWeight;
			inFile >> m_CurvatureSpeedExponent;
			inFile >> m_LaplacianWeight;
			inFile >> m_LaplacianSpeedExponent;
			inFile >> m_AdvectionWeight;
			inFile >> m_AdvectionSpeedExponent;
			inFile >> m_RMSError;
			inFile >> type;
			m_Solver = SolverType(type);
			inFile.close();
			// set UI values
			_PSF_Advection->value( m_AdvectionWeight);
			_DFD_Advection->value( m_AdvectionWeight);
			_GeodAdvection->value( m_AdvectionWeight);
			_GeodShapeAdvection->value( m_AdvectionWeight);
			_PSF_Propagation->value( m_PropagationWeight);
			_DFD_Propagation->value( m_PropagationWeight);
			_GeodPropagation->value( m_PropagationWeight);
			_GeodShapePropagation->value( m_PropagationWeight);
			_PSF_Curvature->value( m_CurvatureWeight);
			_DFD_Curvature->value( m_CurvatureWeight);
			_GeodCurvature->value( m_CurvatureWeight);
			_GeodShapeCurvature->value( m_CurvatureWeight);
			_PSF_RMS_Error->value( m_RMSError);
			_DFD_RMS_Error->value( m_RMSError);
			_GeodRMSError->value( m_RMSError);
			_GeodShapeRMSError->value( m_RMSError);
			_SegmentTabGroup->value( m_Solver);
		}
		else
		{
			alert( "Error opening %s", _fileChooser->value());
		}
	}
}

void
SegParameters::saveSegParameters()
{
	if ( _fileChooser == 0)
	{
		_fileChooser = new FileChooser( _directory.c_str(), "*.par",
					FileChooser::CREATE, "Save Snake Parameters");
	}
	else
	{
		_fileChooser->type( FileChooser::CREATE);
		_fileChooser->label("Save Snake Parameters");
		_fileChooser->filter( "*.par");
	}
	_fileChooser->show( 300, 300);

	while ( _fileChooser->visible())
		fltk::wait();

	if ( _fileChooser->count() > 0)
	{
		ofstream outFile;
		outFile.open( _fileChooser->value(), ios::out);
		if ( outFile)
		{
			outFile << "Snake Parameters" << endl;
			outFile << m_TimeStepFactor << endl;
			outFile << m_SpeedType << endl;
			outFile << m_AutomaticTimeStep << endl;
			outFile << m_PropagationWeight << endl;
			outFile << m_PropagationSpeedExponent << endl;
			outFile << m_CurvatureWeight << endl;
			outFile << m_CurvatureSpeedExponent << endl;
			outFile << m_LaplacianWeight << endl;
			outFile << m_LaplacianSpeedExponent << endl;
			outFile << m_AdvectionWeight << endl;
			outFile << m_AdvectionSpeedExponent << endl;
			outFile << m_RMSError << endl;
			outFile << m_Solver << endl;
			outFile.close();
		}
		else
		{
			alert( "Error opening %s", _fileChooser->value());
		}
	}
}

