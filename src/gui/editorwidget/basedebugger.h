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

#ifndef BASEDEBUGGER_H
#define BASEDEBUGGER_H

#include <QHash>

#include "compiler.h"

class BaseDebugger : public Compiler    // Base Class for all debuggers
{
        friend class eMcu;

    Q_OBJECT
    public:
        BaseDebugger( CodeEditor* parent, OutPanelText* outPane );
        ~BaseDebugger();

        virtual bool upload();

        void stepDebug();
        void stepFromLine( int line, bool over=false );

        void setLstType( int type ) { m_lstType = type; }
        void setLangLevel( int level ) { m_langLevel = level; }

        void setLineToFlash( int line, int addr );

        int getValidLine( int pc );
        bool isMappedLine( int line );

        QString getVarType( QString var );

        //QList<int> getSubLines() { return m_subLines; }

        virtual void getInfoInFile( QString line );

        static QString getValue( QString line, QString word );

        int flashToSourceSize() { return m_flashToSource.size(); }
        
        bool m_stepOver;

    protected:
        virtual void preProcess() override;
        virtual bool postProcess() override;

        bool isNoValid( QString line );

        virtual void getSubs(){;}
        virtual void setBoardName( QString board ){ m_board = board; }

        bool m_debugStep;
        bool m_over;
        int  m_prevLine;
        int  m_exitPC;

        int m_processorType;
        int m_lastLine;
        int m_lstType;   // Bit0: 0 doesn't use ":" (gpasm ), 1 uses ":" (avra, gavrasm)
                         // Bit1: position of flash address (0 or 1)
        int m_langLevel; // 0 for asm, 1 for high level
        int m_codeStart;

        QString m_appPath;

        //QStringList m_subs;
        //QList<int>  m_subLines;
        
        QHash<QString, QString> m_typesList;
        QHash<QString, QString> m_varTypes; // Variable name-Type got from source file
        QHash<int, int> m_flashToSource;    // Map flash adress to Source code line
        QHash<int, int> m_sourceToFlash;    // Map .asm code line to flash adress
        QHash<QString, int> m_functions;    // Function name list->start Address
        QList<int>          m_funcAddr;     // Function start Address list
};

#endif

