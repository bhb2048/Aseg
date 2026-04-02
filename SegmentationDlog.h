//
//	SegmentationDlog.h - subclass for SegmentationUI
//
//	27-dec-11  bhb
//	Modified:
//	01-nov-13  bhb	renamed from 'Segmentation' to 'SegmentationDlog'
//	05-dec-13  bhb	replaced autoTerminate() w/ setSeconds()
//	14-apr-15  bhb	add getIterations(), getSeconds()
//	
#ifndef _SegmentationDlog_h
#define	_SegmentationDlog_h

#include "UI/SegmentationUI.h"

class Aseg;

typedef unsigned int uint;

class SegmentationDlog : public SegmentationUI
{
	public:
		SegmentationDlog( Aseg *p);
		virtual ~SegmentationDlog();

		enum State	{ RUN, STOP	};
		
		Aseg *	parent()	{ return _parent;	}
		fltk::Output *	textOutput()	{ return _textOutput;	}
		
		uint	getStepSize()			{ return _stepSize;	}
		void	setIterations( int i)	{ _iterationOutput->value( double(i));	}
		uint	getIterations()			{ return uint(_iterationOutput->value());	}
		void	setSeconds( int s)		{ _secondsOutput->value( double(s));	}
		uint	getSeconds()			{ return uint(_secondsOutput->value());	}
		
	private:
		Aseg *		_parent;

		uint		_stepSize;
		
		virtual void restartSegmentation();
		virtual void startSegmentation();
		virtual void stopSegmentation();
		virtual void stepSegmentation();
		virtual void setStepSize();
};
#endif
