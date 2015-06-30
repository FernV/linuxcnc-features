LinuxCNC Features v2 - native realtime CAM for LinuxCNC

Welcome to this advanced version of LinuxCNC-Features.

It is an almost complete re-write but with a few changes to your files
 it will take full advantage of all the new features
 there is also some settings you can do in the beginning of features.py


1. Testing Stand Alone
--------------------------------------------------------------------------------
You are invited to test it and all you have to do is make sure you have python-lxml installed.
To install python-lxml, simply open a terminal and copy the following command :

sudo apt-get install python-lxml

Download LinuxCNC-Features in the folder of your choice (recommended is ~/linuxcnc-features)

Make sure features.py is executable

Open a terminal in features directory and type ./features.py
(Or simply double click on it)
Default catalog is mill

And yes, start linuxcnc in a terminal with this command :
'linuxcnc ~/linuxcnc/configs/sim.axis/axis.ini' (not axis_mm.ini).
It does not matter if you start it before or after features.py

Open some examples and if all is well, enjoy.


2. Using Embedded
--------------------------------------------------------------------------------
Install by copying the following lines in a terminal

1. Install python-lxml
	sudo apt-get install python-lxml 


2. Create links into /usr/share/pyshared/gladevcp/ 
	(change directory as required)
	
	cd /usr/share/pyshared/gladevcp/
	sudo ln /home/$USER/linuxcnc-features/features.py -s
	sudo ln /home/$USER/linuxcnc-features/features.glade -s
	

3. Change hal_pythonplugin.py in /usr/share/pyshared/gladevcp/

	sudo gedit /usr/share/pyshared/gladevcp/hal_pythonplugin.py

	Add this new line anywhere
		from features import Features


4. Change hal_python.xml in /usr/share/glade3/catalogs glade3 can be glade2

	sudo gedit /usr/share/glade3/catalogs/hal_python.xml

	Find :  <glade-widget-classes>
	(it is in the beginning)
	Add after:
		<glade-widget-class name="Features" generic-name="features" title="features">
		    <properties>
		        <property id="size" query="False" default="1" visible="False"/>
		        <property id="spacing" query="False" default="0" visible="False"/>
		        <property id="homogeneous" query="False" default="0" visible="False"/>
		    </properties>
		</glade-widget-class>

	Find :  <glade-widget-group name="python" title="HAL Python">
	Add after :
		<glade-widget-class-ref name="Features"/>

	IMPORTANT NOTE : when linuxcnc updates, it recreates directories and if features do not load
	you will have to redo steps 2, 3 and 4
	
	
5. Create links into /usr/lib/pymodules/python2.7/gladevcp

	cd /usr/lib/pymodules/python2.7/gladevcp
	sudo ln /usr/share/pyshared/gladevcp/features.py -s
	sudo ln /usr/share/pyshared/gladevcp/features.glade -s


6. Open your linuxcnc ini file (follow the example in ~/linuxcnc-features/mill.ini)
	I recommend you use ~/linuxcnc=features/mill.ini or ~/linuxcnc-features/lathe.ini
	find 
		[DISPLAY]
 		make sure DISPLAY = axis
	add this line anywhere in the section
		GLADEVCP = -U --catalog=mill features.ui
	or
		GLADEVCP = -U --catalog=lathe features.ui		

	in the same section, find PROGRAM_PREFIX
	if it does not exist, add it and give it the value
		PROGRAM_PREFIX = ./scripts

	find 
		[RS274NGC] section
	add the path to ngc sub that will be called from your ngc scripts that you load in linuxcnc
		SUBROUTINE_PATH = ./lib

	features.ui is allready in ~/linuxcnc-features directory
	
	NOTE: I highly recommend you use ~/linuxcnc-features/ for your config directory
	to avoid creating a structure on your own


You should now be able to start linuxcnc and have Features vcp embedded in axis
THE COMMAND IS : linuxcnc /home/$USER/linuxcnc-features/mill.ini


7. Optional: Translations
	Translation will work in Stand Alone AND Embedded modes

	Make links in your system locale directories to translation files
	
	cd /usr/share/locale/<<<YOUR LOCALE>>>/LC_MESSAGES
	
	sudo ln /<full path to features sourse>/locale/<<<YOUR LOCALE>>>/LC_MESSAGES/linuxcnc-features.mo -s

	Example:
	cd /usr/share/locale/fr/LC_MESSAGES
	sudo ln /home/$USER/linuxcnc/features/locale/fr/LC_MESSAGES/linuxcnc-features.mo -s


3.	Extending subroutines
--------------------------------------------------------------------------------

1. Param subsitutions
	"#param_name" can be used to substitude parameters from the feature. 
	"#self_id" - unique id made of feature Name + smallest integer id. 

2. Eval and exec	
	<eval>"hello world!"</eval>	
	everything inside &lt;eval&gt; tag will be passed
	trought python's eval function. 
	
	<exec>print "hello world"</exec>
	allmost the same but will take all printed data.
	
	you can use self as feature's self.

3. Including Gcode
	G-code ngc files can be included by using one of the following functions:
	
	[DEFINITIONS]
	content = 
		<eval>self.include_once("rotate-xy.ngc")</eval>
		<eval>self.include("rotate-xy.ngc")</eval>

4. Data types
	[SUBROUTINE] type should be lower case, short, without space. Ex : circle, rect, probe-dn

	Params types are : string, float, int, bool (or boolean), header (or hdr), combo, items
	Note : you can change string, float and int types on the fly with the context menu. 
	This is usefull with variables
	
	when using a value like #<var_name> use string because if will evaluate to 0 if int used or 0.0 if float
	
	Examples in ini/mill/fv_circle.ini and others
	
	Note : icons and images only need name.ext
	
--------------------------------------------------------------------------------
