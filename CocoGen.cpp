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

#include "CocoGen.h"
#include "EbnfAnalyzer.h"
#include "FirstFollowSet.h"
#include "GenUtils.h"
#include "SynTreeGen.h"
#include <QFile>
#include <QTextStream>
#include <QtDebug>

CocoGen::CocoGen():d_tbl(0)
{

}

bool CocoGen::generate(const QString& atgPath, EbnfSyntax* syn, FirstFollowSet* tbl, bool buildAst )
{
    // see http://ssw.jku.at/Coco/

    if( syn == 0 || syn->getOrderedDefs().isEmpty() )
        return false;

    d_tbl = tbl;

    const EbnfSyntax::Definition* root = syn->getOrderedDefs()[0];

    QFile f(atgPath);
    f.open( QIODevice::WriteOnly );
    QTextStream out(&f);
    out.setCodec("Latin-1");

    out << "// This file was automatically generated by EbnfStudio; don't modify it!" << endl;
    if( buildAst )
    {
        out << "#include <QStack>" << endl;
        out << "#include <VlSynTree.h>" << endl;
        out << "COMPILER " << root->d_tok.d_val.toBa() << endl;

        out << endl;


        out << "\tVl::SynTree d_root;" << endl;
        out << "\tQStack<Vl::SynTree*> d_stack;" << endl;

        out << "\t" << "void addTerminal() {" << endl;
        /*
        out << "\t\t" << "if( " // TODO: braucht es das?
               " d_cur.d_type != Vl::Tok_Semi "
               "&& d_cur.d_type != Vl::Tok_Lbrack "
               "&& d_cur.d_type != Vl::Tok_Rbrack "
               "&& d_cur.d_type != Vl::Tok_Hash "
               "&& d_cur.d_type != Vl::Tok_At "
               "){" << endl;
               */
        out << "\t\t" << "Vl::SynTree* n = new Vl::SynTree( d_cur ); d_stack.top()->d_children.append(n);" << endl;
        out << "\t}" << endl;

    }
    out << endl;

    out << "TOKENS" << endl;

    SynTreeGen::TokenNameValueList tokens = SynTreeGen::generateTokenList(syn);

    for( int t = 0; t < tokens.size(); t++ )
    {
        out << "  " << tokenName(tokens[t].first);
        if( tokens[t].second.isEmpty() )
            out << "_";
        out << endl;
    }
    out << endl;

    out << "PRODUCTIONS" << endl;
    out << endl;

    // Fall i=0
    if( !syn->getOrderedDefs().isEmpty() )
    {
        const EbnfSyntax::Definition* d = syn->getOrderedDefs().first();
        out << GenUtils::escapeDollars( d->d_tok.d_val ) << " = " << endl << "    ";
        if( buildAst )
            out << "(. d_stack.push(&d_root); .) (";
        writeNode( out, d->d_node, true, buildAst );
        if( buildAst )
            out << ") (. d_stack.pop(); .) ";
        out << endl << "    ." << endl << endl;
    }

    // Fall i>0
    for( int i = 1; i < syn->getOrderedDefs().size(); i++ )
    {
        const EbnfSyntax::Definition* d = syn->getOrderedDefs()[i];

        if( d->d_tok.d_op == EbnfToken::Skip || ( i != 0 && d->d_usedBy.isEmpty() ) )
            continue;
        if( d->d_node == 0 )
            continue;

        out << GenUtils::escapeDollars( d->d_tok.d_val ) << " = " << endl << "    ";
        const bool transparent = d->d_tok.d_op == EbnfToken::Transparent;
        if( buildAst && !transparent )
            out << "(. Vl::SynTree* n = new Vl::SynTree( Vl::SynTree::R_" << GenUtils::escapeDollars( d->d_tok.d_val ) <<
                   ", d_next ); d_stack.top()->d_children.append(n); d_stack.push(n); .) ( ";
        writeNode( out, d->d_node, true, buildAst );

        if( buildAst && !transparent )
            out << ") (. d_stack.pop(); .) ";

        out << endl << "    ." << endl << endl;
    }

    out << "END " << root->d_tok.d_val.toBa() << " ." << endl;
}

void CocoGen::writeNode( QTextStream& out, EbnfSyntax::Node* node, bool topLevel, bool buildAst )
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
        out << "[ ";
        break;
    case EbnfSyntax::Node::ZeroOrMore:
        out << "{ ";
        break;
    }

    switch( node->d_type )
    {
    case EbnfSyntax::Node::Terminal:
        out << tokenName( node->d_tok.d_val ) << " ";
        if( buildAst )
            out << "(. addTerminal(); .) ";
        break;
    case EbnfSyntax::Node::Nonterminal:
        if( node->d_def == 0 || node->d_def->d_node == 0 )
        {
            // pseudoterminal
            out << tokenName(node->d_tok.d_val) << " ";
            if( buildAst )
                out << "(. addTerminal(); .) ";
        }else
            out << GenUtils::escapeDollars(node->d_tok.d_val) << " ";
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
            writeNode( out, node->d_subs[i], false, buildAst );
        }
        break;
    case EbnfSyntax::Node::Sequence:
        for( int i = 0; i < node->d_subs.size(); i++ )
        {
            if( node->d_subs[i]->d_type == EbnfSyntax::Node::Predicate )
                handlePredicate( out, node->d_subs[i], node );
            else
                writeNode( out, node->d_subs[i], false, buildAst );
        }
        break;
    case EbnfSyntax::Node::Predicate:
        qWarning() << "Coco::writeNode: EbnfSyntax::Node::Predicate";
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
        out << "] ";
        break;
    case EbnfSyntax::Node::ZeroOrMore:
        out << "} ";
        break;
    }
}

void CocoGen::handlePredicate(QTextStream& out,EbnfSyntax::Node* pred, EbnfSyntax::Node* sequence)
{
#if 1
    const int ll = pred->getLlk();
    if( ll > 0 )
    {
        EbnfAnalyzer::LlkNodes llkNodes;
        EbnfAnalyzer::calcLlkFirstSet( ll,  0, llkNodes,sequence, d_tbl );
//        qDebug() << "**** lookAheadNext result";
//        for(int n = 0; n < llkNodes.size(); n++ )
//            qDebug() << n << llkNodes[n];
        out << "IF( ";
        for( int i = 0; i < llkNodes.size(); i++ )
        {
            if( i != 0 )
                out << "&& ";
            if( llkNodes[i].size() > 1 )
                out << "( ";
            EbnfSyntax::NodeSet::const_iterator j;
            for( j = llkNodes[i].begin(); j != llkNodes[i].end(); ++j )
            {
                if( j != llkNodes[i].begin() )
                    out << "|| ";
                out << "peek(" << i+1 << ") == _" << tokenName( (*j).d_node->d_tok.d_val ) << " ";
            }
            if( llkNodes[i].size() > 1 )
                out << ") ";
        }
        out << ") ";
    }else
        qWarning() << "CocoGen unknown predicate" << sequence->d_tok.d_val.toStr();
#endif
#if 0
    if( node->d_attr.startsWith("LA:") )
    {
        AttrParser p;
        Syntax::Node* n = (Syntax::Node*)p.parse( node->d_attr.mid(3) );
        if( n )
        {
            out << "IF( ";
            writeAttrNode(out,n,1);
            out << ") ";
            delete n;
        }
    }
#endif
}

QString CocoGen::tokenName(const QByteArray& str)
{
    return QLatin1String("T_") + GenUtils::symToString(str);
}