#include <iostream>
#include "axon-axopatch1D.h"

// Create wrapper for QComboBox. Options go red when changed and black when 'Set DAQ' is hit.
AxoPatchComboBox::AxoPatchComboBox(QWidget *parent) : QComboBox(parent) {
	QObject::connect(this, SIGNAL(activated(int)), this, SLOT(redden(void)));
}

AxoPatchComboBox::~AxoPatchComboBox(void) {}

void AxoPatchComboBox::blacken(void) {
	this->setStyleSheet("QComboBox { color:black; }");
}

void AxoPatchComboBox::redden(void) {
	this->setStyleSheet("QComboBox { color:red; }");
}

// Create wrapper for spinboxes. Function is analogous to AxoPatchComboBox
// SpinBox was used instead of DefaultGUILineEdit because palette.setBrush(etc...) doesn't change colors when changes are done programmatically. 
AxoPatchSpinBox::AxoPatchSpinBox(QWidget *parent) : QSpinBox(parent) {
	QObject::connect(this, SIGNAL(valueChanged(int)), this, SLOT(redden(void)));
}

AxoPatchSpinBox::~AxoPatchSpinBox(void) {}

void AxoPatchSpinBox::blacken(void) {
	this->setStyleSheet("QSpinBox { color:black; }");
}

void AxoPatchSpinBox::redden(void) {
	this->setStyleSheet("QSpinBox { color:red; }");
}


/* This is the real deal, the definitions for all the AxoPatch functions.
 */
extern "C" Plugin::Object * createRTXIPlugin(void) {
	return new AxoPatch();
};

static DefaultGUIModel::variable_t vars[] = {
	{ "Gain Telegraph", "Input telegraph", DefaultGUIModel::INPUT, }, // telegraph from DAQ used in 'Auto' mode
	{ "Input Channel", "Input channel (#)", DefaultGUIModel::PARAMETER | DefaultGUIModel::INTEGER, }, 
	{ "Output Channel", "Output channel (#)", DefaultGUIModel::PARAMETER | DefaultGUIModel::INTEGER, },
	{ "Headstage Gain", "Headstage gain configuration setting", 
	  DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE, }, 
	{ "Command Sensitivity", "Command sensitivity setting", 
	  DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE, },
	{ "Output Gain", "Scaled output gain setting", DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE, }, 
	{ "Amplifier Mode", "Amplifier mode (vclamp, iclamp, or i=0)", 
	   DefaultGUIModel::PARAMETER | DefaultGUIModel::INTEGER, },
};

static size_t num_vars = sizeof(vars) / sizeof(DefaultGUIModel::variable_t);

// Definition of global function used to get all DAQ devices available. Copied from legacy version of program. -Ansel
static void getDevice(DAQ::Device *d, void *p) {
	DAQ::Device **device = reinterpret_cast<DAQ::Device **>(p);

	if (!*device) *device = d;
}

// Just the constructor. 
AxoPatch::AxoPatch(void) : DefaultGUIModel("AxoPatch 1D Controller", ::vars, ::num_vars) {
	setWhatsThis("<p>Amplifier control module to compensate for scaling properties of the Axon AxoPatch 1D controller. This module essentially acts as an interface that replicates functionality of the control panel, but instead of manually setting gains, etc., you can do it at the press of a button. Note that you will still have to activate the channels via the control panel, though. .</p>");
	DefaultGUIModel::createGUI(vars, num_vars);
	initParameters();
	customizeGUI();
	update( INIT );
	refresh();
	QTimer::singleShot(0, this, SLOT(resizeMe()));
};

AxoPatch::~AxoPatch(void) {};

void AxoPatch::initParameters(void) {
	input_channel = 0;
	output_channel = 1;
	amp_mode = 1;
	output_gain = headstage_gain = 1;
	command_sens = 20;

	device = 0;
	DAQ::Manager::getInstance()->foreachDevice(getDevice, &device);

	// these are amplifier-specific settings. 
	iclamp_ai_gain = 1 / 10; // (1 V/ 10V)
	iclamp_ao_gain = 1e-3 / 10e-12; // (10 pA/mV)
	izero_ai_gain = 1 / 10;  // (1 V/ 10V)
	izero_ao_gain = 0; // No output
	vclamp_ai_gain = 1e-12/1e-3; // (1 mV/pA)
	vclamp_ao_gain = 1 / 20e-3; // (20 mV/V)
};


void AxoPatch::update(DefaultGUIModel::update_flags_t flag) {

	switch(flag) {
		// initialize the parameters and then the GUI. 
		case INIT:
			setParameter("Input Channel", input_channel);
			setParameter("Output Channel", output_channel);
			setParameter("Headstage Gain", headstage_gain);
			setParameter("Command Sensitivity", command_sens);
			setParameter("Output Gain", output_gain);
			setParameter("Amplifier Mode", amp_mode);

			updateGUI();
			break;
		
		case MODIFY:
			input_channel = getParameter("Input Channel").toInt();
			output_channel = getParameter("Output Channel").toInt();
			output_gain = getParameter("Output Gain").toDouble();
			headstage_gain = getParameter("Headstage Gain").toDouble();
			command_sens = getParameter("Command Sensitivity").toDouble();
			ampButtonGroup->button(amp_mode)->setStyleSheet("QRadioButton {font: normal;}");
			amp_mode = getParameter("Amplifier Mode").toInt();

			updateDAQ();
			updateGUI(); // Only needed here because doLoad doesn't update the gui on its own. 
			             // Yes, it does cause a bug with the headstage option, but it doesn't 
			             // matter to anything other than whatever OCD tendencies we all 
			             // probably have. You're welcome. -Ansel

			// blacken the GUI to reflect that changes have been saved to variables.
			inputBox->blacken();
			outputBox->blacken();
			headstageBox->blacken();
			outputGainBox->blacken();
			commandSensBox->blacken();
			break;

		// disable the all the buttons in Auto mode. Auto mode does everything on its own.
		case PAUSE:
			inputBox->setEnabled(true);
			outputBox->setEnabled(true);
			outputGainBox->setEnabled(true);
			headstageBox->setEnabled(true);
			commandSensBox->setEnabled(true);
			iclampButton->setEnabled(true);
			vclampButton->setEnabled(true);
			modifyButton->setEnabled(true);
			break;
		
		//when unpaused, return gui functionality to the user.
		case UNPAUSE:
			inputBox->setEnabled(false);
			outputBox->setEnabled(false);
			outputGainBox->setEnabled(false);
			commandSensBox->setEnabled(false);
			headstageBox->setEnabled(false);
			iclampButton->setEnabled(false);
			vclampButton->setEnabled(false);
			modifyButton->setEnabled(false);
			break;

		default:
			break;
	}
}

void AxoPatch::execute(void) {

	// Check current gain
	if (input(0) <= .6 ) temp_gain = .5;
	else if (input(0) <= 1.0) temp_gain = 1.0;
	else if (input(0) <= 1.4) temp_gain = 2.0;
	else if (input(0) <= 1.8) temp_gain = 5.0;
	else if (input(0) <= 2.2) temp_gain = 10.0;
	else if (input(0) <= 2.6) temp_gain = 20.0;
	else if (input(0) <= 3.0) temp_gain = 50.0;
	else output_gain = 100.0;

	if (temp_gain != output_gain) {
		settings_changed = true;
		output_gain = temp_gain;
	}
}

// used solely for initializing the gui when the module is opened or loaded via doLoad
void AxoPatch::updateGUI(void) {
	// set the i/o channels
	inputBox->setValue(input_channel);
	outputBox->setValue(output_channel);

	// set the headstage gain. gain=1 defaults to the first combobox option. 
	if (headstage_gain == .1) {
		headstageBox->setCurrentIndex(2);
	} else {
		headstageBox->setCurrentIndex(0);
	}

	if (output_gain == .5) {
		outputGainBox->setCurrentIndex(0);
	} else if (output_gain == 1) {
		outputGainBox->setCurrentIndex(1);
	} else if (output_gain == 2) {
		outputGainBox->setCurrentIndex(2);
	} else if (output_gain == 5) {
		outputGainBox->setCurrentIndex(3);
	} else if (output_gain == 10) {
		outputGainBox->setCurrentIndex(4);
	} else if (output_gain == 20) {
		outputGainBox->setCurrentIndex(5);
	} else if (output_gain == 50) {
		outputGainBox->setCurrentIndex(6);
	} else if (output_gain == 100) {
		outputGainBox->setCurrentIndex(7);
	} else {
		outputGainBox->setCurrentIndex(0);
	}

	if (command_sens == 20) {
		commandSensBox->setCurrentIndex(0);
	} else if (command_sens == 1) {
		commandSensBox->setCurrentIndex(1);
	} else {
		commandSensBox->setCurrentIndex(0);
	}
	
	ampButtonGroup->button(amp_mode)->setChecked(true);
	ampButtonGroup->button(amp_mode)->setStyleSheet("QRadioButton { font: bold; }");
}

// update the text in the block made by createGUI whenever the mode option changes. 
void AxoPatch::updateMode(int value) {
	parameter["Amplifier Mode"].edit->setText(QString::number(value));
	parameter["Amplifier Mode"].edit->setModified(true);
}

// update the gain text in the hidden block made by createGUI whenever the combobox item changes
void AxoPatch::updateOutputGain(int value) {
	double temp_value;

	switch(value) {
		case 0:
			temp_value = .5;
			break;
		case 1:
			temp_value = 1;
			break;
		case 2:
			temp_value = 2;
			break;
		case 3:
			temp_value = 5;
			break;
		case 4:
			temp_value = 10;
			break;
		case 5:
			temp_value = 20;
			break;
		case 6:
			temp_value = 50;
			break;
		case 7:
			temp_value = 100;
			break;
		default:
			temp_value = 1;
			break;
	}

	parameter["Output Gain"].edit->setText(QString::number(temp_value));
	parameter["Output Gain"].edit->setModified(true);
}


// updates the headstage gain in the exact same manner as updateOutputGain
void AxoPatch::updateHeadstageGain(int value) {
	double temp_value;

	switch(value) {
		case 2: 
			temp_value = .1;
			break;

		default:
			temp_value = 1;
			break;
	}
	
	parameter["Headstage Gain"].edit->setText(QString::number(temp_value));
	parameter["Headstage Gain"].edit->setModified(true);
}

void AxoPatch::updateCommandSens(int value) {
	double temp_value;

	switch(value) {
		case 0:
			temp_value = 20;
			break;
		case 1:
			temp_value = 1;
			break;
		default:
			temp_value = 20;
			std::cout<<"ERROR: default called in updateCommansSens"<<std::endl;
			break;
	}

	parameter["Command Sensitivity"].edit->setText(QString::number(temp_value));
	parameter["Command Sensitivity"].edit->setModified(true);
}

// updates the output channel text whenever the value in the gui spinbox changes.
void AxoPatch::updateOutputChannel(int value) {
	parameter["Output Channel"].edit->setText(QString::number(value));
	parameter["Output Channel"].edit->setModified(true);
}

// updates input channel
void AxoPatch::updateInputChannel(int value) {
	parameter["Input Channel"].edit->setText(QString::number(value));
	parameter["Input Channel"].edit->setModified(true);
}

// updates the DAQ settings whenever the 'Set DAQ' button is pressed or when Auto mode detects a need for it.
void AxoPatch::updateDAQ(void) {
	if (!device) return;

	switch(amp_mode) {
		case 1: //IClamp
			device->setAnalogRange(DAQ::AI, input_channel, 0);
			if (getActive()) {
				device->setAnalogGain(DAQ::AI, input_channel, iclamp_ai_gain/output_gain*headstage_gain);
			} else {
				device->setAnalogGain(DAQ::AI, input_channel, iclamp_ai_gain/output_gain);
			}
			device->setAnalogGain(DAQ::AO, output_channel, iclamp_ao_gain*headstage_gain*command_sens);
			device->setAnalogCalibration(DAQ::AO, output_channel);
			device->setAnalogCalibration(DAQ::AI, input_channel);
			break;
		
		case 2: //I = 0
			device->setAnalogRange(DAQ::AI, input_channel, 0);
			if (getActive()) {
				device->setAnalogGain(DAQ::AI, input_channel, iclamp_ai_gain/output_gain*headstage_gain);
			} else {
				device->setAnalogGain(DAQ::AI, input_channel, iclamp_ai_gain/output_gain);
			}
			device->setAnalogGain(DAQ::AO, output_channel, 0);
			device->setAnalogCalibration(DAQ::AO, output_channel);
			device->setAnalogCalibration(DAQ::AI, input_channel);
			break;

		case 3: //VClamp
			device->setAnalogRange(DAQ::AI, input_channel, 0);
			if (getActive()) {
				device->setAnalogGain(DAQ::AI, input_channel, vclamp_ai_gain/output_gain*headstage_gain);
			} else {
				device->setAnalogGain(DAQ::AI, input_channel, vclamp_ai_gain/output_gain);
			}
			device->setAnalogGain(DAQ::AO, output_channel, vclamp_ao_gain*command_sens);
			device->setAnalogCalibration(DAQ::AO, output_channel);
			device->setAnalogCalibration(DAQ::AI, input_channel);
			break;

		default:
			std::cout<<"ERROR. Something went horribly wrong.\n The amplifier mode is set to an unknown value"<<std::endl;
			break;
	}
};

/* 
 * Sets up the GUI. It's a bit messy. These are the important things to remember:
 *   1. The parameter/state block created by DefaultGUIModel is HIDDEN. 
 *   2. The Unload button is hidden, Pause is renamed 'Auto', and Modify is renamed 'Set DAQ'
 *   3. All GUI changes are connected to their respective text boxes in the hidden block
 *   4. 'Set DAQ' updates the values of inner variables with GUI choices linked to the text boxes
 */
void AxoPatch::customizeGUI(void) {
	QGridLayout *customLayout = DefaultGUIModel::getLayout();
	
	customLayout->itemAtPosition(1,0)->widget()->setVisible(false);
	DefaultGUIModel::pauseButton->setText("Auto");
	DefaultGUIModel::modifyButton->setText("Set DAQ");
	DefaultGUIModel::unloadButton->setVisible(false);

	// create input spinboxes
	QGroupBox *ioBox = new QGroupBox;
	QHBoxLayout *ioBoxLayout = new QHBoxLayout;
	ioBox->setLayout(ioBoxLayout);
	inputBox = new AxoPatchSpinBox; // this is the QSpinBox wrapper made earlier
	inputBox->setRange(0,100);
	outputBox = new AxoPatchSpinBox;
	outputBox->setRange(0,100);
	
	QLabel *inputBoxLabel = new QLabel("Input");
	QLabel *outputBoxLabel = new QLabel("Output");
	ioBoxLayout->addWidget(inputBoxLabel);
	ioBoxLayout->addWidget(inputBox);
	ioBoxLayout->addWidget(outputBoxLabel);
	ioBoxLayout->addWidget(outputBox);

	// create gain, headstage, and amplifer mode comboboxes
	QGroupBox *comboBoxGroup = new QGroupBox;
	QFormLayout *comboBoxLayout = new QFormLayout;
	headstageBox = new AxoPatchComboBox;
		// this part copied from the legacy version. -Ansel
	headstageBox->insertItem( 0, trUtf8("\x50\x61\x74\x63\x68\x20\xce\xb2\x3d\x31") );
	headstageBox->insertItem( 1, trUtf8( "\x57\x68\x6f\x6c\x65\x20\x43\x65\x6c\x6c\x20\xce\xb2\x3d\x31" ) );
	headstageBox->insertItem( 2, trUtf8( "\x57\x68\x6f\x6c\x65\x20\x43\x65\x6c\x6c\x20\xce\xb2\x3d\x30\x2e\x31" ) );
	outputGainBox = new AxoPatchComboBox;
	outputGainBox->insertItem( 0, tr("0.5") );
	outputGainBox->insertItem( 1, tr("1") );
	outputGainBox->insertItem( 2, tr("2") );
	outputGainBox->insertItem( 3, tr("5") );
	outputGainBox->insertItem( 4, tr("10") );
	outputGainBox->insertItem( 5, tr("20") );
	outputGainBox->insertItem( 6, tr("50") );
	outputGainBox->insertItem( 7, tr("100") );
	commandSensBox = new AxoPatchComboBox;
	commandSensBox->insertItem( 0, tr("20 mV/V"));
	commandSensBox->insertItem( 1, tr("1 mV/V"));
	comboBoxLayout->addRow("Output Gain", outputGainBox);
	comboBoxLayout->addRow("Headstage", headstageBox);
	comboBoxLayout->addRow("Sensitivity", commandSensBox);
	comboBoxGroup->setLayout(comboBoxLayout);

	// create amp mode groupbox
	QGroupBox *ampModeBox = new QGroupBox;
	QHBoxLayout *ampModeBoxLayout = new QHBoxLayout;
	ampModeBox->setLayout(ampModeBoxLayout);
	ampButtonGroup = new QButtonGroup;
	iclampButton = new QRadioButton("IClamp");
	iclampButton->setCheckable(true);
	ampButtonGroup->addButton(iclampButton, 1);
	izeroButton = new QRadioButton("I = 0");
	izeroButton->setCheckable(true);
	ampButtonGroup->addButton(izeroButton, 2);
	vclampButton = new QRadioButton("VClamp");
	vclampButton->setCheckable(true);
	ampButtonGroup->addButton(vclampButton, 3);
	ampModeBoxLayout->addWidget(iclampButton);
	ampModeBoxLayout->addWidget(izeroButton);
	ampModeBoxLayout->addWidget(vclampButton);
	ampButtonGroup->setExclusive(true);

	// add widgets to custom layout
	customLayout->addWidget(ioBox, 0, 0);
	customLayout->addWidget(comboBoxGroup, 2, 0);
	customLayout->addWidget(ampModeBox, 3, 0);
	setLayout(customLayout);

	// connect the widgets to the signals
	QObject::connect(ampButtonGroup, SIGNAL(buttonPressed(int)), this, SLOT(updateMode(int)));
	QObject::connect(outputGainBox, SIGNAL(activated(int)), this, SLOT(updateOutputGain(int)));
	QObject::connect(headstageBox, SIGNAL(activated(int)), this, SLOT(updateHeadstageGain(int)));
	QObject::connect(commandSensBox, SIGNAL(activated(int)), this, SLOT(updateCommandSens(int)));
	QObject::connect(inputBox, SIGNAL(valueChanged(int)), this, SLOT(updateInputChannel(int)));
	QObject::connect(outputBox, SIGNAL(valueChanged(int)), this, SLOT(updateOutputChannel(int)));
}


// overload the refresh function to display Auto mode settings and update the DAQ (when in Auto)
void AxoPatch::refresh(void) {
	for (std::map<QString, param_t>::iterator i = parameter.begin(); i!= parameter.end(); ++i) {
		if (i->second.type & (STATE | EVENT)) {
			i->second.edit->setText(QString::number(getValue(i->second.type, i->second.index)));
			palette.setBrush(i->second.edit->foregroundRole(), Qt::darkGray);
			i->second.edit->setPalette(palette);
		} else if ((i->second.type & PARAMETER) && !i->second.edit->isModified() && i->second.edit->text() != *i->second.str_value) {
			i->second.edit->setText(*i->second.str_value);
		} else if ((i->second.type & COMMENT) && !i->second.edit->isModified() && i->second.edit->text() != QString::fromStdString(getValueString(COMMENT, i->second.index))) {
			i->second.edit->setText(QString::fromStdString(getValueString(COMMENT, i->second.index)));
		}
	}
	pauseButton->setChecked(!getActive());

	if (getActive()) {
		if (settings_changed) {
			updateDAQ();

			outputGainBox->setEnabled(true);
			if (output_gain == .5) {
				outputGainBox->setCurrentIndex(0);
			} else if (output_gain == 1) {
				outputGainBox->setCurrentIndex(1);
			} else if (output_gain == 2) {
				outputGainBox->setCurrentIndex(2);
			} else if (output_gain == 5) {
				outputGainBox->setCurrentIndex(3);
			} else if (output_gain == 10) {
				outputGainBox->setCurrentIndex(4);
			} else if (output_gain == 20) {
				outputGainBox->setCurrentIndex(5);
			} else if (output_gain == 50) {
				outputGainBox->setCurrentIndex(6);
			} else if (output_gain == 100) {
				outputGainBox->setCurrentIndex(7);
			} else {
				outputGainBox->setCurrentIndex(0);
			}
			outputGainBox->setEnabled(false);
		
			settings_changed = false;
		}
	}
}
