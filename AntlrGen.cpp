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

#include "AntlrGen.h"
#include "SynTreeGen.h"
#include "GenUtils.h"
#include "EbnfAnalyzer.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDir>

bool AntlrGen::generate(const QString& atgPath, EbnfSyntax* syn)
{
    if( syn == 0 || syn->getOrderedDefs().isEmpty() )
        return false;

    const EbnfSyntax::Definition* root = syn->getOrderedDefs()[0];

    QFile f(atgPath);
    f.open( QIODevice::WriteOnly );
    QTextStream out(&f);
    out.setCodec("Latin-1");

    //QFileInfo info(atgPath);

    out << "// This file was automatically generated by EbnfStudio; don't modify it!" << endl << endl;

    out << "grammar " << root->d_tok.d_val.toBa() << ";" << endl << endl;
    out << "options {" << endl;
    out << "    language = Cpp;" << endl;
    //out << "    tokenVocab = " << info.baseName() << ";" << endl;
    out << "}" << endl << endl;

    out << "tokens {" << endl;
    const QByteArrayList tokens = GenUtils::orderedTokenList( EbnfAnalyzer::collectAllTerminalStrings( syn ) ) +
            EbnfAnalyzer::collectAllTerminalProductions( syn );
    for( int t = 0; t < tokens.size(); t++ )
    {
//        if( tokens[t].second.isEmpty() )
//            continue;
        out << "\t" << tokenName(tokens[t]) << "='" << QByteArray::number(t) << "';" << endl;
    }
    out << "}" << endl << endl;

    out << endl;

    for( int i = 0; i < syn->getOrderedDefs().size(); i++ )
    {
        const EbnfSyntax::Definition* d = syn->getOrderedDefs()[i];
        if( d->d_tok.d_op == EbnfToken::Skip || ( i != 0 && d->d_usedBy.isEmpty() ) )
            continue;
        if( d->d_node == 0 )
            continue;
        out << ruleName( d->d_tok.d_val ) << " : " << endl << "    ";
        writeNode( out, d->d_node, true );
        out << endl << "    ;" << endl << endl;
    }

    /*
    QFile f2( info.absoluteDir().absoluteFilePath( info.baseName() + ".tokens") );
    f2.open( QIODevice::WriteOnly );
    QTextStream out2(&f2);
    out2.setCodec("Latin-1");
    SynTreeGen::TokenNameValueList tokens = SynTreeGen::generateTokenList(syn);

    for( int t = 0; t < tokens.size(); t++ )
    {
        if( tokens[t].second.isEmpty() )
            continue;
        out2 << tokenName(tokens[t].first) << " = " << t << endl;
    }
    */
}

void AntlrGen::writeNode(QTextStream& out, EbnfSyntax::Node* node, bool topLevel)
{
    if( node == 0 )
        return;

    if( node->d_tok.d_op == EbnfToken::Skip )
        return;
    if( node->d_def && node->d_def->d_tok.d_op == EbnfToken::Skip )
        return;

    switch( node->d_quant )
    {
    case EbnfSyntax::Node::One:
        if( !topLevel && node->d_type == EbnfSyntax::Node::Alternative )
            out << "( ";
        else if( !topLevel && node->d_type == EbnfSyntax::Node::Sequence && !node->d_tok.d_val.isEmpty() )
            out << "( ";
        break;
    case EbnfSyntax::Node::ZeroOrOne:
    case EbnfSyntax::Node::ZeroOrMore:
        out << "( ";
        break;
    }
    switch( node->d_type )
    {
    case EbnfSyntax::Node::Terminal:
        out << tokenName( node->d_tok.d_val ) << " ";
        break;
    case EbnfSyntax::Node::Nonterminal:
        if( node->d_def == 0 || node->d_def->d_node == 0 )
            out << tokenName(node->d_tok.d_val) << " ";
        else
            out << ruleName(node->d_tok.d_val) << " ";
        break;
    case EbnfSyntax::Node::Alternative:
        for( int i = 0; i < node->d_subs.size(); i++ )
        {
            if( i != 0 )
            {
                if( topLevel )
                    out << endl << "    | ";
                else
                    out << "| ";
            }
            writeNode( out, node->d_subs[i], false );
        }
        break;
    case EbnfSyntax::Node::Sequence:
        for( int i = 0; i < node->d_subs.size(); i++ )
        {
            writeNode( out, node->d_subs[i], false );
        }
        break;
    default:
        break;
    }
    switch( node->d_quant )
    {
    case EbnfSyntax::Node::One:
        if( !topLevel && node->d_type == EbnfSyntax::Node::Alternative )
            out << ") ";
        else if( !topLevel && node->d_type == EbnfSyntax::Node::Sequence && !node->d_tok.d_val.isEmpty() )
            out << ") ";
        break;
    case EbnfSyntax::Node::ZeroOrOne:
        out << ")? ";
        break;
    case EbnfSyntax::Node::ZeroOrMore:
        out << ")* ";
        break;
    }
}

QString AntlrGen::tokenName(const QByteArray& str)
{
    QString tok = GenUtils::symToString(str).toUpper();
    if( !tok.isEmpty() && tok[0].isDigit() )
        tok = QChar('T') + tok;
    return tok;
}

QString AntlrGen::ruleName(const QByteArray& str)
{
    return GenUtils::escapeDollars( str ).toLower();
}

AntlrGen::AntlrGen()
{

}

