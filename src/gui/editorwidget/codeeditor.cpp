/***************************************************************************
 *   Copyright (C) 2012 by santiago González                               *
 *   santigoro@gmail.com                                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.  *
 *                                                                         *
 ***************************************************************************/

#include "codeeditor.h"
#include "outpaneltext.h"
#include "compilerprop.h"
#include "highlighter.h"
#include "mcuinterface.h"
#include "basedebugger.h"
#include "mcubase.h"
#include "mainwindow.h"
#include "simulator.h"
#include "circuitwidget.h"
#include "editorwindow.h"
#include "simuapi_apppath.h"
#include "utils.h"

QStringList CodeEditor::m_picInstr = QString("addlw addwf andlw andwf bcf bov bsf btfsc btg btfss clrf clrw clrwdt comf decf decfsz goto incf incfsz iorlw iorwf movf movlw movwf reset retfie retlw return rlf rrfsublw subwf swapf xorlw xorwf").split(" ");
QStringList CodeEditor::m_avrInstr = QString("add adc adiw sub subi sbc sbci sbiw andi ori eor com neg sbr cbr dec tst clr ser mul rjmp ijmp jmp rcall icall ret reti cpse cp cpc cpi sbrc sbrs sbic sbis brbs brbc breq brne brcs brcc brsh brlo brmi brpl brge brlt brhs brhc brts brtc brvs brvc brie brid mov movw ldi lds ld ldd sts st std lpm in out push pop lsl lsr rol ror asr swap bset bclr sbi cbi bst bld sec clc sen cln sez clz sei cli ses cls sev clv set clt seh clh wdr").split(" ");

bool    CodeEditor::m_showSpaces = false;
bool    CodeEditor::m_spaceTabs  = false;
bool    CodeEditor::m_driveCirc  = false;
int     CodeEditor::m_fontSize = 13;
int     CodeEditor::m_tabSize = 4;
QString CodeEditor::m_sintaxPath;
QString CodeEditor::m_compilsPath;
QString CodeEditor::m_tab;
QFont   CodeEditor::m_font = QFont();

QList<CodeEditor*> CodeEditor::m_documents;

CodeEditor::CodeEditor( QWidget* parent, OutPanelText* outPane )
          : QPlainTextEdit( parent )
          , Updatable()
{
    m_compDialog = NULL;
    m_documents.append( this );

    m_outPane   = outPane;
    m_lNumArea  = new LineNumberArea( this );
    m_hlighter  = new Highlighter( document() );
    
    m_debugger = NULL;
    m_debugLine = 0;
    m_brkAction = 0;

    m_isCompiled= false;
    m_driveCirc = false;

    m_help = "";
    m_state = DBG_STOPPED;

    setFont( m_font );

    QPalette p = palette();
    p.setColor( QPalette::Base, QColor( 255, 255, 249) );
    p.setColor( QPalette::Text, QColor( 0, 0, 0) );
    setPalette( p );

    connect( this, SIGNAL( blockCountChanged( int )),
             this, SLOT( updateLineNumberAreaWidth( int )), Qt::UniqueConnection );

    connect( this, SIGNAL( updateRequest( QRect,int )),
             this, SLOT( updateLineNumberArea( QRect,int )), Qt::UniqueConnection);

    connect( this, SIGNAL( cursorPositionChanged() ),
             this, SLOT( highlightCurrentLine() ), Qt::UniqueConnection);
    
    setLineWrapMode( QPlainTextEdit::NoWrap );
    updateLineNumberAreaWidth( 0 );
    highlightCurrentLine();
}
CodeEditor::~CodeEditor()
{
    m_documents.removeAll( this );
}

void CodeEditor::setDebugger( BaseDebugger* debugger )
{
    if( m_debugger ) delete m_debugger;
    m_debugger = debugger;
}

void CodeEditor::setFile( const QString filePath )
{
    m_isCompiled= false;
    if( m_file == filePath ) return;

    if( m_debugger ) delete m_debugger;
    m_debugger = NULL;

    m_outPane->appendLine( "-------------------------------------------------------" );
    m_outPane->appendLine( tr(" File: ")+filePath+"\n" );

    m_file = filePath;
    QDir::setCurrent( m_file );

    QString extension = getFileExt( filePath );

    if( extension == ".gcb" )
    {
        m_hlighter->readSintaxFile( m_sintaxPath + "gcbasic.sintax" );
        m_debugger = EditorWindow::self()->createDebugger( "GcBasic", this );
    }
    else if( extension == ".cpp"
          || extension == ".c"
          || extension == ".ino"
          || extension == ".h" )
    {
        m_hlighter->readSintaxFile( m_sintaxPath + "cpp.sintax" );
        if( extension == ".ino" )
            m_debugger = EditorWindow::self()->createDebugger( "Arduino", this );
    }
    else if( extension == ".asm" ) // We should identify if pic or avr asm
    {
        int isPic = 0;
        int isAvr = 0;
        
        isPic = getSintaxCoincidences( m_file, m_picInstr );
        if( isPic < 50 ) isAvr = getSintaxCoincidences( m_file, m_avrInstr );

        m_outPane->appendText( tr("File recognized as: ") );

        if( isPic > isAvr )   // Is Pic
        {
            m_outPane->appendLine( "Pic asm\n" );
            m_hlighter->readSintaxFile( m_sintaxPath + "pic14asm.sintax" );
            m_debugger = EditorWindow::self()->createDebugger( "GpAsm", this );
        }
        else if( isAvr > isPic )  // Is Avr
        {
            m_outPane->appendLine( "Avr asm\n" );
            m_hlighter->readSintaxFile( m_sintaxPath + "avrasm.sintax" );
            m_debugger = EditorWindow::self()->createDebugger( "Avra", this );
        }
        else m_outPane->appendLine( "Unknown\n" );
    }
    else if( extension == ".xml"
         ||  extension == ".html"
         ||  extension == ".package"
         ||  extension == ".mcu"
         ||  extension == ".sim1"
         ||  extension == ".simu" )
    {
        m_hlighter->readSintaxFile( m_sintaxPath + "xml.sintax" );
    }
    else if( getFileName( m_file ).toLower() == "makefile"  )
    {
        m_hlighter->readSintaxFile( m_sintaxPath + "makef.sintax" );
    }
    else if( extension == ".sac" )
    {
        //m_debugger = new B16AsmDebugger( this, m_outPane );
    }
    if( !m_debugger ) m_debugger = EditorWindow::self()->createDebugger( "None", this );
}

int CodeEditor::getSintaxCoincidences( QString& fileName, QStringList& instructions )
{
    QStringList lines = fileToStringList( fileName, "CodeEditor::getSintaxCoincidences" );

    int coincidences = 0;

    for( QString line : lines )
    {
        if( line.isEmpty()      ) continue;
        if( line.startsWith("#")) continue;
        if( line.startsWith(";")) continue;
        if( line.startsWith(".")) continue;
        line =line.toLower();
        
        for( QString instruction : instructions )
        {
            if( line.contains( QRegExp( "\\b"+instruction+"\\b" ) ))
                coincidences++;
            
            if( coincidences > 50 ) break;
    }   }
    return coincidences;
}

void CodeEditor::compile( bool debug )
{
    if( document()->isModified() ) EditorWindow::self()->save();
    m_debugLine  = -1;
    update();

    m_isCompiled = false;
    
    m_outPane->appendLine( "-------------------------------------------------------" );
    int error = m_debugger->compile( debug );

    if( error == 0 )
    {
        m_outPane->appendLine( "\n"+tr("     SUCCESS!!! Compilation Ok")+"\n" );
        m_isCompiled = true;
        return;
    }
    m_outPane->appendLine( "\n"+tr("     ERROR!!! Compilation Failed")+"\n" );

    if( error > 0 ) // goto error line number
    {
        m_debugLine = error; // Show arrow in error line
        updateScreen();
}   }

void CodeEditor::upload()
{
    if( McuBase::self() && m_file.endsWith(".hex") )// is an .hex file, upload to proccessor
    {
        m_outPane->appendLine( "\n"+tr("Uploading: ")+"\n"+m_file);
        McuBase::self()->load( m_file );
        return;
    }
    if( !m_isCompiled ) compile();
    if( !m_isCompiled ) return;
    m_debugger->upload();
}

bool CodeEditor::initDebbuger()
{
    m_outPane->appendLine( "-------------------------------------------------------\n" );
    m_outPane->appendLine( tr("Starting Debbuger...")+"\n" );

    bool error = false;
    m_state = DBG_STOPPED;
    
    compile( true );
    if     ( !m_isCompiled )         error = true; // Error compiling
    else if( !m_debugger->upload() ) error = true; // Error Loading Firmware

    m_outPane->appendLine( "\n" );

    if( error ) stopDebbuger();
    else{                                         // OK: Start Debugging
        EditorWindow::self()->enableStepOver( m_debugger->m_stepOver );
        Simulator::self()->addToUpdateList( this );
        McuInterface::self()->setDebugging( true );
        reset();
        setDriveCirc( m_driveCirc );
        CircuitWidget::self()->powerCircDebug( m_driveCirc );

        m_outPane->appendLine( tr("Debugger Started ")+"\n" );
        setReadOnly( true );
    }
    return ( m_state == DBG_PAUSED );
}

void CodeEditor::runToBreak()
{
    if( m_state == DBG_STOPPED ) return;
    m_state = DBG_RUNNING;
    if( m_driveCirc ) Simulator::self()->resumeSim();
    McuInterface::self()->stepOne( m_debugLine );
}

void CodeEditor::step( bool over )
{
    if( m_state == DBG_RUNNING ) return;

    if( over ){
        addBreakPoint( m_debugLine+1 );
        EditorWindow::self()->run();
    }else {
        m_state = DBG_STEPING;
        McuInterface::self()->stepOne( m_debugLine );
        if( m_driveCirc ) Simulator::self()->resumeSim();
}   }

void CodeEditor::stepOver()
{
    QList<int> subLines = m_debugger->getSubLines();
    bool over = subLines.contains( m_debugLine ) ? true : false;
    step( over );
}

void CodeEditor::lineReached( int line ) // Processor reached PC related to source line
{
    m_debugLine = line;

    if( ( m_state == DBG_RUNNING )             // We are running to Breakpoint
     && !m_brkPoints.contains( m_debugLine ) ) // Breakpoint not reached, Keep stepping
    {
        McuInterface::self()->stepOne( m_debugLine );
        return;
    }
    EditorWindow::self()->pause(); // EditorWindow: calls this->pause as well

    int cycle = McuInterface::self()->cycle();
    m_outPane->appendLine( tr("Clock Cycles: ")+QString::number( cycle-m_lastCycle ));
    m_lastCycle = cycle;
    updateScreen();
}

void CodeEditor::stopDebbuger()
{
    if( m_state > DBG_STOPPED )
    {
        m_debugLine = 0;
        
        CircuitWidget::self()->powerCircOff();
        McuInterface::self()->setDebugging( false );
        Simulator::self()->remFromUpdateList( this );
        
        m_state = DBG_STOPPED;
        setReadOnly( false );
        updateScreen();
    }
    m_outPane->appendLine( "\n"+tr("Debugger Stopped ")+"\n" );
}

void CodeEditor::pause()
{
    if( m_state < DBG_STEPING )  return;
    if( m_driveCirc ) Simulator::self()->pauseSim();

    m_resume = m_state;
    m_state  = DBG_PAUSED;
}

void CodeEditor::reset()
{
    if( m_state == DBG_RUNNING ) pause();

    McuBase::self()->reset();
    m_debugLine = 1;
    m_lastCycle = 0;
    m_state = DBG_PAUSED;

    updateScreen();
}

void CodeEditor::addBreakPoint( int line )
{
    if( m_state == DBG_RUNNING ) return;
    line = m_debugger->getValidLine( line );
    if( !m_brkPoints.contains( line ) ) m_brkPoints.append( line );
}

void CodeEditor::setDriveCirc( bool drive )
{
    m_driveCirc = drive;
    
    if( m_state == DBG_PAUSED )
    {
        if( drive ) Simulator::self()->pauseSim();
}   }

void CodeEditor::updateScreen()
{
    setTextCursor( QTextCursor(document()->findBlockByLineNumber( m_debugLine-1 )));
    ensureCursorVisible();
    update();
}

void CodeEditor::readSettings() // Static
{
    m_sintaxPath  = SIMUAPI_AppPath::self()->availableDataFilePath("codeeditor/sintax/");
    m_compilsPath = SIMUAPI_AppPath::self()->availableDataFilePath("codeeditor/compilers/");

    m_font.setFamily("Ubuntu Mono");
    m_font.setWeight( 50 );
    m_font.setFixedPitch( true );
    m_font.setPixelSize( m_fontSize );

    QSettings* settings = MainWindow::self()->settings();

    if( settings->contains( "Editor_show_spaces" ) )
        setShowSpaces( settings->value( "Editor_show_spaces" ).toBool() );

    if( settings->contains( "Editor_tab_size" ) )
        setTabSize( settings->value( "Editor_tab_size" ).toInt() );
    else setTabSize( 4 );

    if( settings->contains( "Editor_font_size" ) )
        setFontSize( settings->value( "Editor_font_size" ).toInt() );

    bool spacesTab = false;
    if( settings->contains( "Editor_spaces_tabs" ) )
        spacesTab = settings->value( "Editor_spaces_tabs" ).toBool();

    setSpaceTabs( spacesTab );
}

void CodeEditor::setFontSize( int size )
{
    m_fontSize = size;
    m_font.setPixelSize( size );
    MainWindow::self()->settings()->setValue( "Editor_font_size", QString::number(m_fontSize) );

    for( CodeEditor* doc : m_documents )
    {
        doc->setFont( m_font );
        doc->setTabSize( m_tabSize );
}   }

void CodeEditor::setTabSize( int size )
{
    m_tabSize = size;
    MainWindow::self()->settings()->setValue( "Editor_tab_size", QString::number(m_tabSize) );

    for( CodeEditor* doc : m_documents )
    {
        doc->setTabStopWidth( m_tabSize*m_fontSize*2/3 );
        if( m_spaceTabs ) doc->setSpaceTabs( true );
}   }

void CodeEditor::setShowSpaces( bool on )
{
    m_showSpaces = on;

    for( CodeEditor* doc : m_documents )
    {
        QTextOption option = doc->document()->defaultTextOption();

        if( on ) option.setFlags(option.flags() | QTextOption::ShowTabsAndSpaces);
        else     option.setFlags(option.flags() & ~QTextOption::ShowTabsAndSpaces);

        doc->document()->setDefaultTextOption(option);
    }
    if( m_showSpaces )
         MainWindow::self()->settings()->setValue( "Editor_show_spaces", "true" );
    else MainWindow::self()->settings()->setValue( "Editor_show_spaces", "false" );
}

void CodeEditor::setSpaceTabs( bool on )
{
    m_spaceTabs = on;

    if( on ) { m_tab = ""; for( int i=0; i<m_tabSize; i++) m_tab += " "; }
    else m_tab = "\t";

    if( m_spaceTabs )
         MainWindow::self()->settings()->setValue( "Editor_spaces_tabs", "true" );
    else MainWindow::self()->settings()->setValue( "Editor_spaces_tabs", "false" );
}

void CodeEditor::keyPressEvent( QKeyEvent* event )
{
    if( event->key() == Qt::Key_Plus && (event->modifiers() & Qt::ControlModifier) )
    {
        setFontSize( m_fontSize+1 );
    }
    else if( event->key() == Qt::Key_Minus && (event->modifiers() & Qt::ControlModifier) )
    {
        setFontSize( m_fontSize-1 );
    }
    else if( event->key() == Qt::Key_Tab )
    {
        if( textCursor().hasSelection() ) indentSelection( false );
        else                              insertPlainText( m_tab );
    }
    else if( event->key() == Qt::Key_Backtab )
    {
        if( textCursor().hasSelection() ) indentSelection( true );
        else textCursor().movePosition( QTextCursor::PreviousCharacter, QTextCursor::MoveAnchor , m_tab.size() );
    }
    else{
        int tabs = 0;
        if( event->key() == Qt::Key_Return )
        {
            int n0 = 0;
            int n = m_tab.size();
            QString line = textCursor().block().text();
            
            while(1)
            {
                QString part = line.mid( n0, n );
                if( part == m_tab ) { n0 += n; tabs += 1; }
                else break;
        }   }
        QPlainTextEdit::keyPressEvent( event );
        
        if( event->key() == Qt::Key_Return )
            for( int i=0; i<tabs; i++ ) insertPlainText( m_tab );
}   }

void CodeEditor::contextMenuEvent( QContextMenuEvent* event )
{
    QMenu *menu = createStandardContextMenu();
    menu->addSeparator();

    QAction* reloadAction = menu->addAction(QIcon(":/reload.png"), tr("Reload Document"));
    connect( reloadAction, SIGNAL( triggered()),
             EditorWindow::self(), SLOT(reload()), Qt::UniqueConnection );

    menu->exec( event->globalPos() );
}

void CodeEditor::compProps()
{
    if( !m_compDialog )
    {
        m_compDialog = new CompilerProp( this );
        m_compDialog->setDebugger( m_debugger );
    }
    m_compDialog->show();
}

void CodeEditor::setDevice( QString device )
{
    if( m_compDialog ) m_compDialog->setDevice( device );
}

int CodeEditor::lineNumberAreaWidth()
{
    int digits = 1;
    int max = qMax( 1, blockCount() );
    while( max >= 10 ) { max /= 10; ++digits; }
    return  fontMetrics().height() + fontMetrics().width( QLatin1Char( '9' ) ) * digits;
}

void CodeEditor::updateLineNumberArea( const QRect &rect, int dy )
{
    if( dy ) m_lNumArea->scroll( 0, dy );
    else     m_lNumArea->update( 0, rect.y(), m_lNumArea->width(), rect.height() );
    if( rect.contains( viewport()->rect() ) ) updateLineNumberAreaWidth( 0 );
}

void CodeEditor::resizeEvent( QResizeEvent *e )
{
    QPlainTextEdit::resizeEvent( e );
    QRect cr = contentsRect();
    m_lNumArea->setGeometry( QRect( cr.left(), cr.top(), lineNumberAreaWidth(), cr.height() ) );
}

void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    if( !isReadOnly() )
    {
        QTextEdit::ExtraSelection selection;
        QColor lineColor = QColor( 250, 240, 220 );

        selection.format.setBackground( lineColor );
        selection.format.setProperty( QTextFormat::FullWidthSelection, true );
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append( selection );
    }
    setExtraSelections( extraSelections );
}

void CodeEditor::lineNumberAreaPaintEvent( QPaintEvent *event )
{
    QPainter painter( m_lNumArea );
    painter.fillRect( event->rect(), Qt::lightGray );

    QTextBlock block = firstVisibleBlock();

    int blockNumber = block.blockNumber();
    int top       = (int)blockBoundingGeometry(block).translated(contentOffset()).top();
    int fontSize  = fontMetrics().height();

    while( block.isValid() && top <= event->rect().bottom() )
    {
        int blockSize = (int)blockBoundingRect( block ).height();
        int bottom = top + blockSize;

        if( block.isVisible() && bottom >= event->rect().top() )
        {
            int lineNumber = blockNumber + 1;
            // Check if there is a new breakpoint request from context menu
            int pos = m_lNumArea->lastPos;
            if( pos > top && pos < bottom)
            {
                if     ( m_brkAction == 1 ) addBreakPoint( lineNumber );
                else if( m_brkAction == 2 ) remBreakPoint( lineNumber );
                m_brkAction = 0;
                m_lNumArea->lastPos = 0;
            }
            if(( m_state > DBG_STOPPED )
              && m_brkPoints.contains( lineNumber )) // Draw breakPoint icon
            {
                painter.setBrush( QColor(Qt::yellow) );
                painter.setPen( Qt::NoPen );
                painter.drawRect( 0, top, fontSize, fontSize );
            }
            if( lineNumber == m_debugLine ) // Draw debug line icon
                painter.drawImage( QRectF(0, top, fontSize, fontSize), QImage(":/finish.png") );
            // Draw line number
            QString number = QString::number( lineNumber );
            painter.setPen( Qt::black );
            painter.drawText( 0, top, m_lNumArea->width(), fontSize, Qt::AlignRight, number );
        }
        block = block.next();
        top = bottom;
        ++blockNumber;
}   }

void CodeEditor::indentSelection( bool unIndent )
{
    QTextCursor cur = textCursor();
    int a = cur.anchor();
    int p = cur.position();
    
    cur.beginEditBlock();
     
    if( a > p ) std::swap( a, p );
    
    QString str = cur.selection().toPlainText();
    QString str2 = "";
    QStringList list = str.split("\n");
    
    int lines = list.count();
 
    for( int i=0; i<lines; ++i )
    {
        QString line = list[i];

        if( unIndent ) 
        {
            int n = m_tab.size();
            int n1 = n;
            int n2 = 0;
            
            while( n1 > 0 )
            {
                if( line.size() <= n2 ) break;
                QString car = line.at(n2);
                
                if     ( car == " " ) { n1 -= 1; n2 += 1; }
                else if( car == "\t" ) { n1 -= n; if( n1 >= 0 ) n2 += 1; }
                else n1 = 0;
            }
            line.replace( 0, n2, "" );
        }
        else line.insert( 0, m_tab );
        
        if( i < lines-1 ) line += "\n";
        str2 += line;
    }
    cur.removeSelectedText();
    cur.insertText(str2);
    p = cur.position();

    cur.setPosition( a );
    cur.setPosition( p, QTextCursor::KeepAnchor );

    setTextCursor(cur);
    cur.endEditBlock();
}


// ********************* CLASS LineNumberArea **********************************

LineNumberArea::LineNumberArea( CodeEditor *editor ) : QWidget(editor)
{
    m_codeEditor = editor;
}
LineNumberArea::~LineNumberArea(){}

void LineNumberArea::contextMenuEvent( QContextMenuEvent *event)
{
    event->accept();
    if( !m_codeEditor->debugStarted() ) return;
    
    QMenu menu;

    QAction *addBrkAction = menu.addAction( QIcon(":/breakpoint.png"),tr( "Add BreakPoint" ) );
    connect( addBrkAction, SIGNAL( triggered()),
               m_codeEditor, SLOT(slotAddBreak()), Qt::UniqueConnection );

    QAction *remBrkAction = menu.addAction( QIcon(":/nobreakpoint.png"),tr( "Remove BreakPoint" ) );
    connect( remBrkAction, SIGNAL( triggered()),
               m_codeEditor, SLOT(slotRemBreak()), Qt::UniqueConnection );

    menu.addSeparator();

    QAction *clrBrkAction = menu.addAction( QIcon(":/remove.png"),tr( "Clear All BreakPoints" ) );
    connect( clrBrkAction, SIGNAL( triggered()),
               m_codeEditor, SLOT(slotClearBreak()), Qt::UniqueConnection );

    if( menu.exec(event->globalPos()) != 0 ) lastPos = event->pos().y();
}

#include "moc_codeeditor.cpp"
