/*
 * Copyright (C) 2011 Weill Medical College of Cornell University
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <daq.h>
#include <default_gui_model.h>

class AxoPatchComboBox : public QComboBox {

	Q_OBJECT

	public:
		AxoPatchComboBox(QWidget * =0);
		~AxoPatchComboBox(void);
		void blacken(void);

	public slots:
		void redden(void);
};


class AxoPatchSpinBox : public QSpinBox {

	Q_OBJECT

	public:
		AxoPatchSpinBox(QWidget * =0);
		~AxoPatchSpinBox(void);
		void blacken(void);

	public slots:
		void redden(void);
};


class AxoPatch : public DefaultGUIModel {
	
	Q_OBJECT
	
	public:
		AxoPatch(void);
		virtual ~AxoPatch(void);
	
		void initParameters(void);
		void customizeGUI(void);
		void execute(void);
		void updateDAQ(void);
		void updateGUI(void);
		void refresh(void);
	
	protected:
		virtual void update(DefaultGUIModel::update_flags_t);

	private:
		double iclamp_ai_gain; // 1 V/V
		double iclamp_ao_gain; // 2 nA/V) ...hmm
		double izero_ai_gain; // 1 V/V
		double izero_ao_gain; // No output
		double vclamp_ai_gain; // 1 mV/pA
		double vclamp_ao_gain; // 20 mV/V

		int input_channel, output_channel;
		double output_gain, temp_gain, headstage_gain;
		int amp_mode, temp_mode;
		double command_sens;
		
		bool settings_changed;

		DAQ::Device *device;
	
		QRadioButton *iclampButton, *vclampButton, *izeroButton;
		QButtonGroup *ampButtonGroup;
		AxoPatchSpinBox *inputBox, *outputBox;
		AxoPatchComboBox *headstageBox, *outputGainBox, *commandSensBox;
//		QLabel *ampModeLabel;

	private slots:
		void updateMode(int);
		void updateOutputGain(int);
		void updateHeadstageGain(int);
		void updateInputChannel(int);
		void updateOutputChannel(int);
		void updateCommandSens(int);
};
