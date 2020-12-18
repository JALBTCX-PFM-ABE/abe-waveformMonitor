
/*********************************************************************************************

    This is public domain software that was developed by or for the U.S. Naval Oceanographic
    Office and/or the U.S. Army Corps of Engineers.

    This is a work of the U.S. Government. In accordance with 17 USC 105, copyright protection
    is not available for any work of the U.S. Government.

    Neither the United States Government, nor any employees of the United States Government,
    nor the author, makes any warranty, express or implied, without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, or assumes any liability or
    responsibility for the accuracy, completeness, or usefulness of any information,
    apparatus, product, or process disclosed, or represents that its use would not infringe
    privately-owned rights. Reference herein to any specific commercial products, process,
    or service by trade name, trademark, manufacturer, or otherwise, does not necessarily
    constitute or imply its endorsement, recommendation, or favoring by the United States
    Government. The views and opinions of authors expressed herein do not necessarily state
    or reflect those of the United States Government, and shall not be used for advertising
    or product endorsement purposes.
*********************************************************************************************/


/****************************************  IMPORTANT NOTE  **********************************

    Comments in this file that start with / * ! or / / ! are being used by Doxygen to
    document the software.  Dashes in these comment blocks are used to create bullet lists.
    The lack of blank lines after a block of dash preceeded comments means that the next
    block of dash preceeded comments is a new, indented bullet list.  I've tried to keep the
    Doxygen formatting to a minimum but there are some other items (like <br> and <pre>)
    that need to be left alone.  If you see a comment that starts with / * ! or / / ! and
    there is something that looks a bit weird it is probably due to some arcane Doxygen
    syntax.  Be very careful modifying blocks of Doxygen comments.

*****************************************  IMPORTANT NOTE  **********************************/




#ifndef VERSION

#define     VERSION     "PFM Software - waveformMonitor V3.32 - 10/20/17"

#endif

/*! <pre>

    Version 1.0
    Jan C. Depner
    04/05/05

    First working version.


    Version 1.01
    Jan C. Depner
    04/15/05

    Added error message for file open errors (other than command line).  Happy tax day :-(


    Version 1.02
    Jan C. Depner
    04/19/05

    Changed all of the QVBox's to QVBoxLayout's.  This allows the dialogs to auto
    size correctly.  I'm learning ;-)


    Version 1.03
    Jan C. Depner
    05/05/05

    Finally got all of the utility library functions working in C++.  Happy Cinco De Mayo!


    Version 1.1
    Jan C. Depner
    05/18/05

    Modified so that it could be started geoSwath (--geoSwath PPID argument) as well as PFM_ABE.


    Version 1.11
    Jan C. Depner
    05/31/05

    Fixed bug - wasn't setting share->modcode when trying to swap.


    Version 1.2
    Jan C. Depner
    06/30/05

    Added ability to display wave forms as lines or dots.


    Version 1.21
    Jan C. Depner
    08/24/05

    Fix display of reported and corrected depths.


    Version 1.22
    Jan C. Depner
    03/28/06

    Replaced QVGroupBox with QGroupBox and removed QHBox and QVBox to prepare for Qt 4.


    Version 1.23
    Jan C. Depner
    04/10/06

    Added WhatsThis button to prefs and extended dialogs.


    Version 2.0
    Jan C. Depner
    04/12/07

    Qt 4 port.


    Version 2.01
    Jan C. Depner
    05/30/07

    Added alpha values to saved colors.


    Version 2.02
    Jan C. Depner
    08/03/07

    Correctly compute backslope by looking for next rise to end slope computation area.


    Version 2.03
    Jan C. Depner
    08/24/07

    Switched from setGeometry to resize and move.  See Qt4 X11 window geometry documentation.


    Version 3.00
    Jan C. Depner
    09/20/07

    Switched from POSIX shared memory to QSharedMemory.


    Version 3.01
    Jan C. Depner
    10/23/07

    Now honors the CHILD_PROCESS_FORCE_EXIT key.


    Version 3.02
    Jan C. Depner
    01/04/08

    Now uses the parent process ID of the bin viewer plus _pfm or _abe for the shared memory ID's.  This removes the single instance
    per user restriction from ABE.


    Version 3.03
    Jan C. Depner
    04/01/08

    Added acknowledgments to the Help pulldown menu.


    Version 3.04
    Jan C. Depner
    04/07/08

    Replaced single .h and .hpp files from utility library with include of nvutility.h and nvutility.hpp


    Version 3.05
    Jan C. Depner
    04/28/08

    Use save_wave instead of wave_data when computing slopes for the extended display.


    Version 3.06
    Jan C. Depner
    07/15/08

    Removed pfmShare shared memory usage and replaced with abeShare.


    Version 3.07
    Jan C. Depner
    03/20/09

    Reinstated the program and renamed it to waveformMonitor.  This was at the request of the ACE.  The new
    waveMonitor (from USM) eats up too much screen real estate.  Also removed the extended dialog since
    that functionality is being replaced by chartsMonitor.  Added ability to support WLF format.


    Version 3.08
    Jan C. Depner
    04/08/09

    Changes to deal with "flush" argument on all nvmapp.cpp (nvutility library) drawing functions.


    Version 3.09
    Jan C. Depner
    04/23/09

    Changed the acknowledgments help to include Qt and a couple of others.


    Version 3.10
    Jan C. Depner
    04/27/09

    Replaced QColorDialog::getRgba with QColorDialog::getColor.


    Version 3.11
    Jan C. Depner
    06/15/09

    Added support for PFM_CHARTS_HOF_DATA.


    Version 3.12
    Jan C. Depner
    07/29/09

    Added ac_zero_level lines.


    Version 3.13
    Jan C. Depner
    08/06/09

    Added selected point marker on waveforms for WLF data.


    Version 3.14
    Jan C. Depner
    09/11/09

    Fixed getColor calls so that cancel works.  Never forget!


    Version 3.15
    Jan C. Depner
    09/14/09

    Fix the uncorrected vs corrected depth display in KGPS data.  Changed icon to something a little more
    representative.


    Version 3.16
    Jan C. Depner
    09/16/09

    Set killed flag in abe_share when program exits other than from kill switch from parent.


    Version 3.17
    Jan C. Depner
    01/06/11

    Correct problem with the way I was instantiating the main widget.  This caused an intermittent error on Windows7/XP.


    Version 3.18
    Jan C. Depner
    11/30/11

    Converted .xpm icons to .png icons.


    Version 3.20
    Jan C. Depner
    08/21/12

    - Now supports preliminary CZMIL data format.
    - Changed all displayed PMT to Deep and APD to Shallow.  PMT and APD are still used internally for
      historical (hysterical?) reasons.


    Version 3.21
    Jan C. Depner
    09/11/12

    - Check for change of subrecord with CZMIL data since each subrecord has a unique waveform.
    - Never forget!


    Version 3.22
    Jan C. Depner
    10/23/12

    - Now uses interest_point in CZMIL CPF record to place X on waveform.
    - Only 8 shopping days left to retirement!


    Version 3.23
    Jan C. Depner (PFM Software)
    12/09/13

    Switched to using .ini file in $HOME (Linux) or $USERPROFILE (Windows) in the ABE.config directory.  Now
    the applications qsettings will not end up in unknown places like ~/.config/navo.navy.mil/blah_blah_blah on
    Linux or, in the registry (shudder) on Windows.


    Version 3.24
    Jan C. Depner (PFM Software)
    03/17/14

    Removed WLF support.  Top o' the mornin' to ye!


    Version 3.25
    Jan C. Depner (PFM Software)
    03/19/14

    - Straightened up the Open Source acknowledgments.


    Version 3.26
    Jan C. Depner (PFM Software)
    05/27/14

    - Added the new LGPL licensed GSF library to the acknowledgments.


    Version 3.27
    Jan C. Depner (PFM Software)
    07/01/14

    - Replaced all of the old, borrowed icons with new, public domain icons.  Mostly from the Tango set
      but a few from flavour-extended and 32pxmania.


    Version 3.28
    Jan C. Depner (PFM Software)
    07/23/14

    - Switched from using the old NV_INT64 and NV_U_INT32 type definitions to the C99 standard stdint.h and
      inttypes.h sized data types (e.g. int64_t and uint32_t).


    Version 3.29
    Jan C. Depner (PFM Software)
    02/16/15

    - To give better feedback to shelling programs in the case of errors I've added the program name to all
      output to stderr.


    Version 3.30
    Jan C. Depner (PFM Software)
    08/27/16

    - Now uses the same font as all other ABE GUI apps.  Font can only be changed in pfmView Preferences.


    Version 3.31
    Jan C. Depner (PFM Software)
    05/11/17

    - Fixed bug in Windows caused by not reading the HOF header prior to reading the record.


    Version 3.32
    Jan C. Depner (PFM Software)
    10/20/17

    - A bunch of changes to support doing translations in the future.  There is a generic
      waveformMonitor_xx.ts file that can be run through Qt's "linguist" to translate to another language.

</pre>*/
