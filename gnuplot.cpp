/*-------------------------------------------------------------------------*/
/**
  @file		gnuplot.cpp
  @author	N. Devillard
  @modified     D. Karmgard
  @date         Apr 2010
  @version	$Revision: 2.11 $
  @brief	C++ interface to gnuplot.

  gnuplot is a freely available, command-driven graphical display tool for
  Unix. It compiles and works quite well on a number of Unix flavours as
  well as other operating systems. The following module enables sending
  display requests to gnuplot via a c++ class interface
*/

/*--------------------------------------------------------------------------*/

/*
	$Id: gnuplot_i.c,v 2.10 2003/01/27 08:58:04 ndevilla Exp $
	$Author: dkarmgar $
	$Date: 2010-04-14 09:00 $
	$Revision: 2.11 $
 */

/*---------------------------------------------------------------------------
                                Includes
 ---------------------------------------------------------------------------*/

#include "gnuplot.h"

/*---------------------------------------------------------------------------
                                Defines
 ---------------------------------------------------------------------------*/

/** Maximal size of a gnuplot command */
#define GP_CMD_SIZE     	2048

/** Maximal size of a plot title */
#define GP_TITLE_SIZE   	80

/** Maximal size for an equation */
#define GP_EQ_SIZE      	512

/** Maximal size of a name in the PATH */
#define PATH_MAXNAMESZ       4096

/** Define P_tmpdir if not defined (this is normally a POSIX symbol) */
#ifndef P_tmpdir
#define P_tmpdir "."
#endif

/*---------------------------------------------------------------------------
                            Function codes
 ---------------------------------------------------------------------------*/


/*-------------------------------------------------------------------------*/
/**
  @brief	Find out where a command lives in your PATH.
  @param	pname Name of the program to look for.
  @return	pointer to statically allocated character string.

  This is the C equivalent to the 'which' command in Unix. It parses
  out your PATH environment variable to find out where a command
  lives. The returned character string is statically allocated within
  this function, i.e. there is no need to free it. Beware that the
  contents of this string will change from one call to the next,
  though (as all static variables in a function).

  The input character string must be the name of a command without
  prefixing path of any kind, i.e. only the command name. The returned
  string is the path in which a command matching the same name was
  found.

  Examples (assuming there is a prog named 'hello' in the cwd):

  @verbatim
  gnuplot_get_program_path("hello") returns "."
  gnuplot_get_program_path("ls") returns "/bin"
  gnuplot_get_program_path("csh") returns "/usr/bin"
  gnuplot_get_program_path("/bin/ls") returns NULL
  @endverbatim
  
 */
/*-------------------------------------------------------------------------*/
char * GnuPlot::gnuplot_get_program_path(char * pname) {
    int         i, j, lg;
    char    *   path;
    static char buf[PATH_MAXNAMESZ];

    /* Trivial case: try in CWD */
    sprintf(buf, "./%s", pname) ;
    if (access(buf, X_OK)==0) {
        sprintf(buf, ".");
        return buf ;
    }
    /* Try out in all paths given in the PATH variable */
    buf[0] = 0;
    path = getenv("PATH") ;
    if (path!=NULL) {
        for (i=0; path[i]; ) {
            for (j=i ; (path[j]) && (path[j]!=':') ; j++);
            lg = j - i;
            strncpy(buf, path + i, lg);
            if (lg == 0) buf[lg++] = '.';
            buf[lg++] = '/';
            strcpy(buf + lg, pname);
            if (access(buf, X_OK) == 0) {
                /* Found it! */
                break ;
            }
            buf[0] = 0;
            i = j;
            if (path[i] == ':') i++ ;
        }
    } else {
      fprintf(stderr, "PATH variable not set\n");
    }
    /* If the buffer is still empty, the command was not found */
    if (buf[0] == 0) return NULL ;
    /* Otherwise truncate the command name to yield path only */
    lg = strlen(buf) - 1 ;
    while (buf[lg]!='/') {
        buf[lg]=0 ;
        lg -- ;
    }
    buf[lg] = 0;
    return buf ;
}

GnuPlot::GnuPlot() {

  /*-- Create a new session handle --*/
  plot_ctrl = this->gnuplot_init();

  /* Default style, in case the user never sets one */
  this->gnuplot_setstyle((char *)"points") ;

  return;
}

GnuPlot::~GnuPlot() {
  /*-- Close out this particular session --*/
  this->gnuplot_close();
  return;
}

/*-------------------------------------------------------------------------*/
/**
  @brief	Opens up a gnuplot session, ready to receive commands.
  @return	Newly allocated gnuplot control structure.

  This opens up a new gnuplot session, ready for input. The struct
  controlling a gnuplot session should remain opaque and only be
  accessed through the provided functions.

  The session must be closed using gnuplot_close().
 */
/*--------------------------------------------------------------------------*/

gnuplot_ctrl * GnuPlot::gnuplot_init(void) {
  gnuplot_ctrl *  handle;

  if (getenv("DISPLAY") == NULL) {
    fprintf(stderr, "cannot find DISPLAY variable: is it set?\n") ;
  }
  if (gnuplot_get_program_path((char *)"gnuplot")==NULL) {
    fprintf(stderr, "cannot find gnuplot in your PATH");
    return NULL ;
  }

    /* 
     * Structure initialization:
     */
    handle = (gnuplot_ctrl*)malloc(sizeof(gnuplot_ctrl)) ;
    handle->nplots = 0 ;
    handle->ntmp = 0 ;

    handle->gnucmd = popen("gnuplot", "w") ;
    if (handle->gnucmd == NULL) {
        fprintf(stderr, "error starting gnuplot\n") ;
        free(handle) ;
        return NULL ;
    }
    return handle;
}


/*-------------------------------------------------------------------------*/
/**
  @brief	Closes a gnuplot session previously opened by gnuplot_init()
  @param	handle Gnuplot session control handle.
  @return	void

  Kills the child PID and deletes all opened temporary files.
  It is mandatory to call this function to close the handle, otherwise
  temporary files are not cleaned and child process might survive.

 */
/*--------------------------------------------------------------------------*/

void GnuPlot::gnuplot_close(void) {
    int     i ;

    if (pclose(this->plot_ctrl->gnucmd) == -1) {
        fprintf(stderr, "problem closing communication to gnuplot\n") ;
        return ;
    }
    if (this->plot_ctrl->ntmp) {
        for (i=0 ; i<this->plot_ctrl->ntmp ; i++) {
            remove(this->plot_ctrl->to_delete[i]) ;
        }
    }
    free(this->plot_ctrl) ;
    return ;
}


/*-------------------------------------------------------------------------*/
/**
  @brief	Sends a command to an active gnuplot session.
  @param	handle Gnuplot session control handle
  @param	cmd    Command to send, same as a printf statement.

  This sends a string to an active gnuplot session, to be executed.
  There is strictly no way to know if the command has been
  successfully executed or not.
  The command syntax is the same as printf.

  Examples:

  @code
  gnuplot_cmd(g, "plot %d*x", 23.0);
  gnuplot_cmd(g, "plot %g * cos(%g * x)", 32.0, -3.0);
  @endcode

  Since the communication to the gnuplot process is run through
  a standard Unix pipe, it is only unidirectional. This means that
  it is not possible for this interface to query an error status
  back from gnuplot.
 */
/*--------------------------------------------------------------------------*/

void GnuPlot::gnuplot_cmd( char *  cmd, ...) {
    va_list ap ;
    char    local_cmd[GP_CMD_SIZE];

    va_start(ap, cmd);
    vsprintf(local_cmd, cmd, ap);
    va_end(ap);

    strcat(local_cmd, "\n");

    fputs(local_cmd, this->plot_ctrl->gnucmd) ;
    fflush(this->plot_ctrl->gnucmd) ;
    return ;
}

/*-------------------------------------------------------------------------*/
/**
  @brief	Change the plotting style of a gnuplot session.
  @param	h Gnuplot session control handle
  @param	plot_style Plotting-style to use (character string)
  @return	void

  The provided plotting style is a character string. It must be one of
  the following:

  - lines
  - points
  - linespoints
  - impulses
  - dots
  - steps
  - errorbars
  - boxes
  - boxeserrorbars
 */
/*--------------------------------------------------------------------------*/

void GnuPlot::gnuplot_setstyle(char * plot_style) {

   if (strcmp(plot_style, "lines") &&
        strcmp(plot_style, "points") &&
        strcmp(plot_style, "linespoints") &&
        strcmp(plot_style, "impulses") &&
        strcmp(plot_style, "dots") &&
        strcmp(plot_style, "steps") &&
	strcmp(plot_style, "histogram") &&
        strcmp(plot_style, "errorbars") &&
        strcmp(plot_style, "boxes") &&
        strcmp(plot_style, "boxerrorbars")) {
        fprintf(stderr, "warning: unknown requested style: using points\n") ;
        strcpy(this->plot_ctrl->pstyle, "points") ;
    } else {
        strcpy(this->plot_ctrl->pstyle, plot_style) ;
    }
    return ;
}

/*-------------------------------------------------------------------------*/
/**
  @brief	Sets the title of a gnuplot session.
  @param	label Character string to use for plot title.
  @return	void

  Sets the title for a gnuplot session.
 */
/*--------------------------------------------------------------------------*/
void GnuPlot::gnuplot_set_title( char * title ) {
  char cmd[GP_CMD_SIZE];
  sprintf(cmd, "set title '%s'", title);
  gnuplot_cmd(cmd);
  return;
}


/*-------------------------------------------------------------------------*/
/**
  @brief	Sets the x label of a gnuplot session.
  @param	h Gnuplot session control handle.
  @param	label Character string to use for X label.
  @return	void

  Sets the x label for a gnuplot session.
 */
/*--------------------------------------------------------------------------*/

void GnuPlot::gnuplot_set_xlabel(char * label) {
    char    cmd[GP_CMD_SIZE] ;

    sprintf(cmd, "set xlabel \"%s\"", label) ;
    gnuplot_cmd(cmd) ;
    return ;
}


/*-------------------------------------------------------------------------*/
/**
  @brief	Sets the y label of a gnuplot session.
  @param	h Gnuplot session control handle.
  @param	label Character string to use for Y label.
  @return	void

  Sets the y label for a gnuplot session.
 */
/*--------------------------------------------------------------------------*/

void GnuPlot::gnuplot_set_ylabel(char * label) {
    char    cmd[GP_CMD_SIZE] ;

    sprintf(cmd, "set ylabel \"%s\"", label) ;
    gnuplot_cmd(cmd) ;
    return ;
}

/*-------------------------------------------------------------------------*/
/**
  @brief	Resets a gnuplot session (next plot will erase previous ones).
  @param	h Gnuplot session control handle.
  @return	void

  Resets a gnuplot session, i.e. the next plot will erase all previous
  ones.
 */
/*--------------------------------------------------------------------------*/

void GnuPlot::gnuplot_resetplot(void) {
  int     i ;
  if (this->plot_ctrl->ntmp) {
    for (i=0 ; i<this->plot_ctrl->ntmp ; i++) {
      remove(this->plot_ctrl->to_delete[i]) ;
    }
  }
  this->plot_ctrl->ntmp = 0 ;
  this->plot_ctrl->nplots = 0 ;
  return ;
}

/*-------------------------------------------------------------------------*/
/**
  @brief	Plots a 2d graph from a list of doubles.
  @param	handle	Gnuplot session control handle.
  @param	d		Array of doubles.
  @param	n		Number of values in the passed array.
  @param	title	Title of the plot.
  @return	void

  Plots out a 2d graph from a list of doubles. The x-coordinate is the
  index of the double in the list, the y coordinate is the double in
  the list.

  Example:

  @code
    gnuplot_ctrl    *h ;
    double          d[50] ;
    int             i ;

    h = gnuplot_init() ;
    for (i=0 ; i<50 ; i++) {
        d[i] = (double)(i*i) ;
    }
    gnuplot_plot_x(h, d, 50, "parabola") ;
    sleep(2) ;
    gnuplot_close(h) ;
  @endcode
 */
/*--------------------------------------------------------------------------*/
void GnuPlot::gnuplot_plot_x( double *d, int n, char *title ) {

  int     i ;
  int     tmpfd ;
  char    name[128] ;
  char    cmd[GP_CMD_SIZE] ;
  char    line[GP_CMD_SIZE] ;

  if (this->plot_ctrl==NULL || d==NULL || (n<1)) return ;

  /* Open one more temporary file? */
  if (this->plot_ctrl->ntmp == GP_MAX_TMP_FILES - 1) {
    fprintf(stderr,
	    "maximum # of temporary files reached (%d): cannot open more",
	    GP_MAX_TMP_FILES) ;
    return ;
  }

  /* Open temporary file for output   */
  sprintf(name, "%s/gnuplot-i-XXXXXX", P_tmpdir);
  if ((tmpfd=mkstemp(name))==-1) {
    fprintf(stderr,"cannot create temporary file: exiting plot") ;
    return ;
  }

  /* Store file name in array for future deletion */
  strcpy(this->plot_ctrl->to_delete[this->plot_ctrl->ntmp], name) ;
  this->plot_ctrl->ntmp ++ ;
  /* Write data to this file  */
  for (i=0 ; i<n ; i++) {
    sprintf(line, "%g\n", d[i]);
    write(tmpfd, line, strlen(line));
  }
  close(tmpfd) ;

  /* Command to be sent to gnuplot    */
  if (this->plot_ctrl->nplots > 0) {
    strcpy(cmd, "replot") ;
  } else {
    strcpy(cmd, "plot") ;
  }

  if (title == NULL) {
    sprintf(line, "%s \"%s\" with %s", cmd, name, this->plot_ctrl->pstyle) ;
  } else {
    sprintf(line, "%s \"%s\" title \"%s\" with %s", cmd, name,
	    title, this->plot_ctrl->pstyle) ;
  }

  /* send command to gnuplot  */
  gnuplot_cmd(line) ;
  this->plot_ctrl->nplots++ ;
  return ;
}

/*-------------------------------------------------------------------------*/
/**
  @brief	Plot a 2d graph from a list of points.
  @param	handle		Gnuplot session control handle.
  @param	x			Pointer to a list of x coordinates.
  @param	y			Pointer to a list of y coordinates.
  @param	n			Number of doubles in x (assumed the same as in y).
  @param	title		Title of the plot.
  @return	void

  Plots out a 2d graph from a list of points. Provide points through a list
  of x and a list of y coordinates. Both provided arrays are assumed to
  contain the same number of values.

  @code
    gnuplot_ctrl    *h ;
	double			x[50] ;
	double			y[50] ;
    int             i ;

    h = gnuplot_init() ;
    for (i=0 ; i<50 ; i++) {
        x[i] = (double)(i)/10.0 ;
        y[i] = x[i] * x[i] ;
    }
    gnuplot_plot_xy(h, x, y, 50, "parabola") ;
    sleep(2) ;
    gnuplot_close(h) ;
  @endcode
 */
/*--------------------------------------------------------------------------*/

void GnuPlot::gnuplot_plot_xy( double *x, double *y, int n, char *title ) {
 
  int     i ;
  int     tmpfd ;
  char    name[128] ;
  char    cmd[GP_CMD_SIZE] ;
  char    line[GP_CMD_SIZE] ;

  if (this->plot_ctrl==NULL || x==NULL || y==NULL || (n<1)) return ;

  /* Open one more temporary file? */
  if (this->plot_ctrl->ntmp == GP_MAX_TMP_FILES - 1) {
    fprintf(stderr,
	    "maximum # of temporary files reached (%d): cannot open more",
	    GP_MAX_TMP_FILES) ;
    return ;
  }

  /* Open temporary file for output */
  sprintf(name, "%s/gnuplot-i-XXXXXX", P_tmpdir);
  if ((tmpfd=mkstemp(name))==-1) {
    fprintf(stderr,"\ncannot create temporary file: exiting plot\n") ;
    return ;
  }

  /* Store file name in array for future deletion */
  strcpy(this->plot_ctrl->to_delete[this->plot_ctrl->ntmp], name) ;
  this->plot_ctrl->ntmp ++ ;

  /* Write data to this file  */
  for (i=0 ; i<n; i++) {
    sprintf(line, "%g %g\n", x[i], y[i]) ;
    uint bytes = write(tmpfd, line, strlen(line));
    if ( bytes != strlen(line) ) {
      fprintf(stderr, "Write failed: %s", line);
      return;
    }
  }
  close(tmpfd) ;

  /* Command to be sent to gnuplot    */
  if (this->plot_ctrl->nplots > 0) {
    strcpy(cmd, "replot") ;
  } else {
    strcpy(cmd, "plot") ;
  }
  
  if (title == NULL) {
    sprintf(line, "%s \"%s\" with %s", cmd, name, this->plot_ctrl->pstyle) ;
  } else {
    sprintf(line, "%s \"%s\" title \"%s\" with %s", cmd, name,
	    title, this->plot_ctrl->pstyle) ;
  }

  /* send command to gnuplot  */
  gnuplot_cmd(line) ;

  this->plot_ctrl->nplots++ ;
  return ;
}

/*-------------------------------------------------------------------------*/
/**
  @brief	Open a new session, plot a signal, close the session.
  @param	title	Plot title
  @param	style	Plot style
  @param	label_x	Label for X
  @param	label_y	Label for Y
  @param	x		Array of X coordinates
  @param	y		Array of Y coordinates (can be NULL)
  @param	n		Number of values in x and y.
  @return

  This function opens a new gnuplot session, plots the provided
  signal as an X or XY signal depending on a provided y, waits for
  a carriage return on stdin and closes the session.

  It is Ok to provide an empty title, empty style, or empty labels for
  X and Y. Defaults are provided in this case.
 */
/*--------------------------------------------------------------------------*/

void GnuPlot::gnuplot_plot_once( char *title, char *style, char	*label_x,
				 char *label_y, double *x, double *y, int n ) {

	if (x==NULL || n<1) return ;

	if ((this->plot_ctrl = gnuplot_init()) == NULL) return ;

	if (style!=NULL) {
		gnuplot_setstyle(style);
	} else {
	  gnuplot_setstyle((char *)"lines");
	}
	if (label_x!=NULL) {
		gnuplot_set_xlabel(label_x);
	} else {
	  gnuplot_set_xlabel((char *)"X");
	}
	if (label_y!=NULL) {
		gnuplot_set_ylabel(label_y);
	} else {
	  gnuplot_set_ylabel((char *)"Y");
	}
	if (y==NULL) {
		gnuplot_plot_x(x, n, title);
	} else {
		gnuplot_plot_xy(x, y, n, title);
	}
	printf("press ENTER to continue\n");
	while (getchar()!='\n') {}
	gnuplot_close();
	return ;
}

/*-------------------------------------------------------------------------*/
/**
  @brief	Plot a slope on a gnuplot session.
  @param	handle		Gnuplot session control handle.
  @param	a			Slope.
  @param	b			Intercept.
  @param	title		Title of the plot.
  @return	void

  Plot a slope on a gnuplot session. The provided slope has an
  equation of the form y=ax+b

  Example:

  @code
    gnuplot_ctrl    *   h ;
    double              a, b ;

    h = gnuplot_init() ;
    gnuplot_plot_slope(h, 1.0, 0.0, "unity slope") ;
    sleep(2) ;
    gnuplot_close(h) ;
  @endcode
 */
/*--------------------------------------------------------------------------*/
void GnuPlot::gnuplot_plot_slope( double a, double b, char *title ) {
    char    stitle[GP_TITLE_SIZE] ;
    char    cmd[GP_CMD_SIZE] ;

    if (title == NULL) {
        strcpy(stitle, "no title") ;
    } else {
        strcpy(stitle, title) ;
    }

    if (this->plot_ctrl->nplots > 0) {
        sprintf(cmd, "replot %g * x + %g title \"%s\" with %s",
                      a, b, title, this->plot_ctrl->pstyle) ;
    } else {
        sprintf(cmd, "plot %g * x + %g title \"%s\" with %s",
                      a, b, title, this->plot_ctrl->pstyle) ;
    }
    gnuplot_cmd(cmd) ;
    this->plot_ctrl->nplots++ ;
    return ;
}


/*-------------------------------------------------------------------------*/
/**
  @brief	Plot a curve of given equation y=f(x).
  @param	h			Gnuplot session control handle.
  @param	equation	Equation to plot.
  @param	title		Title of the plot.
  @return	void

  Plots out a curve of given equation. The general form of the
  equation is y=f(x), you only provide the f(x) side of the equation.

  Example:

  @code
        gnuplot_ctrl    *h ;
        char            eq[80] ;

        h = gnuplot_init() ;
        strcpy(eq, "sin(x) * cos(2*x)") ;
        gnuplot_plot_equation(h, eq, "sine wave", normal) ;
        gnuplot_close(h) ;
  @endcode
 */
/*--------------------------------------------------------------------------*/

void GnuPlot::gnuplot_plot_equation( char *equation, char *title ) {
    char    cmd[GP_CMD_SIZE];
    char    plot_str[GP_EQ_SIZE] ;
    char    title_str[GP_TITLE_SIZE] ;

    if (title == NULL) {
        strcpy(title_str, "no title") ;
    } else {
        strcpy(title_str, title) ;
    }
    if (this->plot_ctrl->nplots > 0) {
        strcpy(plot_str, "replot") ;
    } else {
        strcpy(plot_str, "plot") ;
    }

    sprintf(cmd, "%s %s title \"%s\" with %s", 
                  plot_str, equation, title_str, this->plot_ctrl->pstyle);
    gnuplot_cmd(cmd) ;
    this->plot_ctrl->nplots++ ;
    return ;
}


/*-------------------------------------------------------------------------*/
/**
  @brief	Plot a histogram of a dataset
  @param	ordinate        x-values for the histogram
  @param        rawdata         y-values of the data
  @param        nbins           number of bins for the histogram
  @param        overflow        include data outside the ordinate range?
  @param	title		Title of the plot.
  @return	void

  generates a histogram in the range [ordinate[0], ordinate[nbins-1]) from
  the raw data passed in. 

**/
/*--------------------------------------------------------------------------*/
void GnuPlot::gnuplot_plot_histogram( double *ordinate, double *rawdata, 
				      const unsigned int nbins,
				      int overflow, char * title ) {
  unsigned int i, j=0;
  double bin[nbins];

  // Zero all the bins.. otherwise we'll get NaNs in any empty bins
  memset(bin, 0, nbins*sizeof(double));

  if ( !rawdata )
    return;

  // build the histogram bin values out of the raw data
  while ( j<nbins && rawdata[j] ) {

    for ( i=0; i<nbins-1; i++ ) {
      if ( overflow ) {
	if ( rawdata[j] <= ordinate[0] ) {
	  bin[0]++;
	} else if ( rawdata[j] >= ordinate[nbins-1] ) {
	  bin[nbins-1]++;
	} else if ( rawdata[j] >= ordinate[i] && rawdata[j] < ordinate[i+1] ) {
	  bin[i]++;
	}
      } else {
	if ( rawdata[j] >= ordinate[i] && rawdata[j] < ordinate[i+1] ) {
	  bin[i]++;
	}
      }
    }
    j++;
  }

  // Now that the counters are set up... make an xy plot out of it
  gnuplot_setstyle((char *)"boxes");
  gnuplot_plot_xy(ordinate, bin, nbins, title);

  return;
}
