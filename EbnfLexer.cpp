/*
* Copyright 2019 Rochus Keller <mailto:me@rochus-keller.ch>
*
* This file is part of the EbnfStudio application.
*
* The following is the license that applies to this copy of the
* application. For a license to use the application under conditions
* other than those described here, please email to me@rochus-keller.ch.
*
* GNU General Public License Usage
* This file may be used under the terms of the GNU General Public
* License (GPL) versions 2.0 or 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE.GPL included in
* the packaging of this file. Please review the following information
* to ensure GNU General Public Licensing requirements will be met:
* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
* http://www.gnu.org/copyleft/gpl.html.
*/

#include "EbnfLexer.h"
#include <QIODevice>
#include <QBuffer>
#include <QFile>
#include <QtDebug>

EbnfLexer::EbnfLexer(QObject *parent) : QObject(parent),
    d_lastToken(EbnfToken::Invalid),d_lineNr(0),d_colNr(0),d_in(0)
{

}

EbnfToken EbnfLexer::nextTokenImp()
{
    if( d_in == 0 )
        return token(EbnfToken::Eof);
    skipWhiteSpace();

    bool potentialProduction = false;
    while( d_colNr >= d_line.size() )
    {
        if( d_in->atEnd() )
        {
            EbnfToken t = token( EbnfToken::Eof, 0 );
            if( d_in->parent() == this )
            {
                d_in->deleteLater();
                d_in = 0;
            }
            return t;
        }
        nextLine();
        potentialProduction = ( skipWhiteSpace() == 0 );
    }

    Q_ASSERT( d_colNr < d_line.size() );
    while( d_colNr < d_line.size() )
    {
        const int ch = quint8(d_line[d_colNr]);

        if( ch == '/' && lookAhead(1) == '/' )
            return token(EbnfToken::Comment, d_line.size() - d_colNr, d_line.mid( d_colNr + 2 ).trimmed() );

        if( ::isalnum(ch) || ch == '$' )
        {
            // Identifier oder Reserved Word
            EbnfToken t = ident();
            if( potentialProduction )
            {
                skipWhiteSpace();
                if( lookAhead(0) == ':' && lookAhead(1) == ':' && lookAhead(2) == '=' )
                {
                    t.d_type = EbnfToken::Production;
                    return t;
                }
            }else
            {
                if( d_kw.contains(t.d_val) )
                    t.d_type = EbnfToken::Keyword;
                return t;
            }
        }else if( ch == '\'' )
        {
            EbnfToken t = literal();
            if( potentialProduction )
            {
                skipWhiteSpace();
                if( lookAhead(0) == ':' && lookAhead(1) == ':' && lookAhead(2) == '=' )
                {
                    t.d_type = EbnfToken::Production;
                    return t;
                }
            }else
                return t;
        }

        if( potentialProduction )
            return token( EbnfToken::Invalid, 0, "production or comment expected" );

        switch( ch )
        {
        case ':':
            if( lookAhead(1) == ':' && lookAhead(2) == '=' )
                return token( EbnfToken::Assig, 3 );
            break;
        case '(':
            return token( EbnfToken::LPar );
        case ')':
            return token( EbnfToken::RPar );
        case '[':
            return token( EbnfToken::LBrack );
        case ']':
            return token( EbnfToken::RBrack );
        case '{':
            return token( EbnfToken::LBrace );
        case '}':
            return token( EbnfToken::RBrace );
        case '|':
            return token( EbnfToken::Bar );
        }
        if( ch == '\\' )
        {
            return attribute();
        }else
        {
            // Error
            return token( EbnfToken::Invalid, 0, QString("unexpected character '%1' %2").arg(char(ch)).arg(ch).toUtf8() );
        }
    }
    Q_ASSERT(false);
    return token(EbnfToken::Invalid);
}

QList<EbnfToken> EbnfLexer::tokens(const QString& code)
{
    return tokens( code.toUtf8() );
}

QList<EbnfToken> EbnfLexer::tokens(const QByteArray& code)
{
    QBuffer in;
    in.setData( code );
    in.open(QIODevice::ReadOnly);
    setStream( &in );

    QList<EbnfToken> res;
    EbnfToken t = nextToken();
    while( t.isValid() )
    {
        res << t;
        t = nextToken();
    }
    return res;
}

void EbnfLexer::setStream(QIODevice* in)
{
    d_in = in;
    d_lineNr = 0;
    d_colNr = 0;
    d_lastToken = EbnfToken::Invalid;
}

bool EbnfLexer::readKeywordsFromFile(const QString& path)
{
    QFile in(path);
    if( !in.open(QIODevice::ReadOnly) )
        return false;
    d_kw.clear();
    QByteArrayList kw = in.readAll().simplified().split(' ');
    for( int i = 0; i < kw.size(); i++ )
    {
        d_kw.insert( EbnfToken::getSym(kw[i]) );
    }
    return true;
}

EbnfToken EbnfLexer::nextToken()
{
    EbnfToken t;
    if( !d_buffer.isEmpty() )
    {
        t = d_buffer.first();
        d_buffer.pop_front();
    }else
        t = nextTokenImp();
    return t;
}

EbnfToken EbnfLexer::peekToken(quint8 lookAhead)
{
    Q_ASSERT( lookAhead > 0 );
    while( d_buffer.size() < lookAhead )
        d_buffer.push_back( nextTokenImp() );
    return d_buffer[ lookAhead - 1 ];
}

int EbnfLexer::skipWhiteSpace()
{
    const int colNr = d_colNr;
    while( d_colNr < d_line.size() && ::isspace( d_line[d_colNr] ) )
        d_colNr++;
    return d_colNr - colNr;
}

EbnfToken EbnfLexer::token(EbnfToken::TokenType tt, int len, const QByteArray& val)
{
    EbnfToken t( tt, d_lineNr, d_colNr + 1, len, val );
    d_lastToken = t;
    d_colNr += len;
    return t;
}

int EbnfLexer::lookAhead(int off) const
{
    if( int( d_colNr + off ) < d_line.size() )
    {
        return d_line[ d_colNr + off ];
    }else
        return 0;
}

void EbnfLexer::nextLine()
{
    d_colNr = 0;
    d_lineNr++;
    d_line = d_in->readLine();

    // see https://de.wikipedia.org/wiki/Zeilenumbruch
    if( d_line.endsWith("\r\n") )
        d_line.chop(2);
    else if( d_line.endsWith('\n') || d_line.endsWith('\r') || d_line.endsWith('\025') )
        d_line.chop(1);

}

EbnfToken EbnfLexer::ident()
{
    int off = 0;
    while( true )
    {
        if( (d_colNr+off) >= d_line.size() ||
                ( !::isalnum(d_line[d_colNr+off]) && d_line[d_colNr+off] != '_' && d_line[d_colNr+off] != '$' ) )
            break;
        else
            off++;
    }
    const QByteArray str = d_line.mid(d_colNr, off );
    EbnfToken t = token( EbnfToken::NonTerm, off, str ); // ob es sich um ein KeyWord handelt, wird später entschieden
    t.d_op = readOp();
    return t;
}

EbnfToken EbnfLexer::attribute()
{
    int off = 1;
    while( true )
    {
        if( (d_colNr+off) >= d_line.size() || d_line[d_colNr+off] == '\\' )
            break;
        else
            off++;
    }
    const QByteArray str = d_line.mid(d_colNr + 1, off - 1 );
    return token( EbnfToken::Predicate, off+1, str );
}

EbnfToken EbnfLexer::literal()
{
    int off = 1;
    while( true )
    {
        if( (d_colNr+off) < d_line.size() && d_line[d_colNr+off] == '\\' )
            off++;
        else if( (d_colNr+off) >= d_line.size() || d_line[d_colNr+off] == '\'' )
            break;
        off++;
    }
    QByteArray str = d_line.mid(d_colNr + 1, off - 1 );
    str.replace("\\'", "'");
    str.replace("\\\\", "\\");
    EbnfToken t = token( EbnfToken::Literal, off+1, str );
    t.d_op = readOp();
    return t;
}

EbnfToken::Handling EbnfLexer::readOp()
{
    if( d_colNr < d_line.size() )
    {
        switch( d_line[d_colNr] )
        {
        case '*':
            d_colNr++;
            return EbnfToken::Transparent;
        case '!':
            d_colNr++;
            return EbnfToken::Keep;
        case '-':
            d_colNr++;
            return EbnfToken::Skip;
        }
    }
    return EbnfToken::Normal;
}


