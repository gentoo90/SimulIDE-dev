/***************************************************************************
 *   Copyright (C) 2018 by santiago González                               *
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

#ifndef VOLTREG_H
#define VOLTREG_H

#include "component.h"
#include "e-resistor.h"

class LibraryItem;

class MAINMODULE_EXPORT VoltReg : public Component, public eResistor
{
    Q_OBJECT
    Q_PROPERTY( double Voltage READ vRef WRITE setVRef DESIGNABLE true USER true )
    
    public:

        VoltReg( QObject* parent, QString type, QString id );
        ~VoltReg();
        
 static Component* construct( QObject* parent, QString type, QString id );
 static LibraryItem* libraryItem();

        virtual QList<propGroup_t> propGroups() override;

        virtual void stamp() override;
        virtual void initialize() override;
        virtual void voltChanged() override;

        double vRef() { return m_value; }
        void setVRef( double vref );
        virtual void setUnit( QString un ) override;

        virtual void paint( QPainter* p, const QStyleOptionGraphicsItem* option, QWidget* widget );

    protected:
        double m_accuracy;
        double m_vRef;
        double m_voltPos;
        double m_voltNeg;
        double m_lastOut;
};


#endif
