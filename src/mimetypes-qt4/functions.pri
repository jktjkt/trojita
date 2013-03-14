###########################################################################################
##      Created using Monkey Studio IDE v1.9.0.1 (1.9.0.1)
##
##  Author    : Filipe Azevedo aka Nox P@sNox <pasnox@gmail.com>
##  Project   : functions.pri
##  FileName  : functions.pri
##  Date      : 2012-07-28T13:13:40
##  License   : LGPL3
##  Comment   : Creating using Monkey Studio RAD
##  Home Page : https://github.com/pasnox/qmake-extensions
##
##  This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
##  WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
##
###########################################################################################

# Return a list of folders being absolute or relative depending of $$1
# $$1: The path where to list folders ($$1 is included)
# $$2: Optional parameter to forbid some filters
defineReplace( getFolders ) {
    q_paths = $$1
    q_filters   = $$2 .svn
    q_folders   =

    for( q_path, q_paths ) {
        command = "ls -RQ1 \"$$q_path\" | grep \":\" | sed \"s/://g\" | sed \"s/'/\\\\\\'/g\""
        macx|win32:command   = ls -R1 \"$$q_path\" | grep \":\" | sed \"s/://g\" | sed \"s/\'/\\\\\\\'/g\" | sed \"s/\\(.*\\)/\\\"\\1\\\"/g\"
        win32:!cb_win32:command  = "for /D /R \"$$q_path\" %i in (*) do @echo \"%i\""

        _q_folders  = $$system( $$command )
        _q_folders *= $$1
        
        _q_folders = $$replace( _q_folders, $$Q_BACK_SLASH, $$Q_SLASH )

        # loop paths
        for( q_folder, _q_folders ) {
            # check filters
            filtered = false

            for( q_filter, q_filters ) {
                result = $$find( q_folder, $$q_filter )
                !isEmpty( result ):filtered = true
            }

            isEqual( filtered, false ):exists( $$q_folder ) {
                q_folders   *= $$q_folder
            }
        }
    }
    
    #message( Getting folders for $$q_paths: $$q_folders )

    return( $$q_folders )
}

# Identical to getFolders except that folders are returned relative to $$3
# $$1: The path where to list folders ($$1 is included)
# $$2: Make relative to $$2
# $$3: Replace $$2 by $$3 in final list
# $$4: Optional parameter to forbid some filters
defineReplace( getRelativeFolders ) {
    q_folders = $$getFolders( $$1, $$4 )
    q_folders = $$replace( q_folders, $$2, $$3 )
    
    #message( Getting relative folders for $$q_paths: $$q_folders )
    
    return( $$q_folders )
}

# Return the project build mode
defineReplace( buildMode ) {
    CONFIG( debug, debug|release ) {
        return( debug )
    } else {
        return( release )
    }
}

# Return a generated target name according to build mode
# $$1: Base target name
# $$2: Optional build mode, if empty get using buildMode()
defineReplace( targetForMode ) {
    q_target    = $$1
    q_mode  = $$2
    isEmpty( q_mode ):q_mode    = $$buildMode()
    
    isEqual( q_mode, release ) {
        q_target    = $$quote( $$q_target )
    } else {
        unix:q_target   = $$quote( $$join( q_target, , , _debug ) )
        else:q_target   = $$quote( $$join( q_target, , , d ) )
    }
    
    return( $$q_target )
}

# Set the project template name
# $$1: Project template name
defineTest( setTemplate ) {
    TEMPLATE  = $$1
    export( TEMPLATE )
}

# Set project target name
# $$1: Target name
# $$: Optional build mode, if empty get using buildMode()
defineTest( setTarget ) {
    TARGET  = $$targetForMode( $$1, $$2 )
    export( TARGET )
}

# Set default directories for temporary files (object, rcc, ui, moc)
# $$1: The temporary directory to use as a base
defineTest( setTemporaryDirectories ) {
    q_mode  = $$buildMode()

    OBJECTS_DIR = $$1/$${Q_TARGET_ARCH}/$${q_mode}/obj
    UI_DIR  = $$1/$${Q_TARGET_ARCH}/$${q_mode}/ui
    MOC_DIR = $$1/$${Q_TARGET_ARCH}/$${q_mode}/moc
    RCC_DIR = $$1/$${Q_TARGET_ARCH}/$${q_mode}/rcc

    export( OBJECTS_DIR )
    export( UI_DIR )
    export( MOC_DIR )
    export( RCC_DIR )
}

# Set project target directory
# $$1: The directory where to generate the project target
defineTest( setTargetDirectory ) {
    DESTDIR = $$1
    export( DESTDIR )
    
    win32|cb_win32:CONFIG( shared ) {
        DLLDESTDIR = $$1
        export( DLLDESTDIR )
    }
}

# Mimic an auto generated file where the content is replaced with qmake variable values
# $$1 = Template source file path
# $$2 = Generated target file path
defineTest( autoGenerateFile ) {
    !build_pass {
        generator.source = $${1}
        generator.target = $${2}
        
        # Replace slashes by back slashes on native windows host
        win32:!cb_win32 {
            generator.source = $$replace( generator.source, $${Q_SLASH}, $${Q_BACK_SLASH} )
            generator.target = $$replace( generator.target, $${Q_SLASH}, $${Q_BACK_SLASH} )
        }
        
        # Delete existing file
        exists( $${generator.target} ) {
            win32:!cb_win32 {
                system( "del $${generator.target}" )
            } else {
                system( "rm $${generator.target}" )
            }
        }
        
        # Get template content
        generator.content = $$cat( $${generator.source}, false )
        
        # Find template variables name
        #generator.variables = $$find( generator.content, "\\$\\$[^\s\$]+" )
        
        # Generate the find variables command
        generator.commands = "grep -E -i -o '\\$\\$[$${Q_OPENING_BRACE}]?[^ $${Q_QUOTE}$]+[$${Q_OPENING_BRACE}]?' $${generator.source}"
        win32:!cb_win32:generator.commands = "grep command not available."
        
        # Get template variables name
        generator.variables = $$system( $${generator.commands} )
        
        #message( cmd: $${generator.commands} )
        #message( Variables: $$generator.variables )

        # Transform each variable
        for( variable, generator.variables ) {
            name = $${variable}
            name = $$replace( name, $${Q_QUOTE}, "" )
            name = $$replace( name, $${Q_DOLLAR}, "" )
            name = $$replace( name, $${Q_OPENING_BRACE}, "" )
            name = $$replace( name, $${Q_CLOSING_BRACE}, "" )
            
            generator.content = $$replace( generator.content, $${Q_DOLLAR}$${Q_DOLLAR}$${Q_OPENING_BRACE}$${name}$${Q_CLOSING_BRACE}, $$eval( $${name} ) )
            generator.content = $$replace( generator.content, $${Q_DOLLAR}$${Q_DOLLAR}$${name}, $$eval( $${name} ) )
            #message( --- Found: $$variable ($$name) - $$eval( $$name ) )
        }
        
        # escape characters that are special for windows echo command
        win32:!cb_win32 {
            generator.content = $$replace( generator.content, "\\^", "^^" )
            generator.content = $$replace( generator.content, "<", "^<" )
            generator.content = $$replace( generator.content, ">", "^>" )
            generator.content = $$replace( generator.content, "\\|", "^|" )
            generator.content = $$replace( generator.content, "&", "^&" )
            # these should be escaped too but qmake values can't be ( or ) so we can't replace...
            #generator.content = $$replace( generator.content, "\\(", "^(" )
            #generator.content = $$replace( generator.content, "\\)", "^)" )
        } else {
            #mac:generator.content = $$replace( generator.content, $${Q_BACK_SLASH}, $${Q_BACK_SLASH}$${Q_BACK_SLASH}$${Q_BACK_SLASH} )
            #else:generator.content = $$replace( generator.content, $${Q_BACK_SLASH}$${Q_BACK_SLASH}, $${Q_BACK_SLASH}$${Q_BACK_SLASH}$${Q_BACK_SLASH} )
            generator.content = $$replace( generator.content, $${Q_BACK_SLASH}$${Q_BACK_SLASH}, $${Q_BACK_SLASH}$${Q_BACK_SLASH}$${Q_BACK_SLASH} )
            generator.content = $$replace( generator.content, $${Q_QUOTE}, $${Q_BACK_SLASH}$${Q_QUOTE} )
        }
        
        message( Generating $${generator.target}... )
        
        win32:!cb_win32 {
            generator.content = $$replace( generator.content, "\\n", ">> $${generator.target} && echo." )
            generator.commands = "echo ^ $${generator.content} >> $${generator.target}"
        } else {
            generator.commands = "echo \" $${generator.content}\" > $${generator.target}"
        }
        
        system( $${generator.commands} )
    }
}
