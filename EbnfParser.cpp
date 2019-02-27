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

#include "EbnfParser.h"
#include "EbnfLexer.h"
#include "EbnfErrors.h"

EbnfParser::EbnfParser(QObject *parent) : QObject(parent),d_lex(0),d_def(0),d_errs(0)
{

}

bool EbnfParser::parse(EbnfLexer* lex)
{
    Q_ASSERT( lex != 0 );
    d_lex = lex;
    if( d_errs )
        d_errs->resetErrCount();

    d_syn = new EbnfSyntax(d_errs);
    d_def = 0;

    EbnfToken t = nextToken();
    while( t.isValid() )
    {
        switch( t.d_type )
        {
        case EbnfToken::Production:
            if( nextToken().d_type != EbnfToken::Assig )
                return error( t, tr("expecing ::= for production") );
            d_def = new EbnfSyntax::Definition(t);
            if( !d_syn->addDef( d_def ) )
            {
                delete d_def;
                return false;
            }
            nextToken();
            if( d_cur.d_type != EbnfToken::Production && d_cur.d_type != EbnfToken::Eof )
                d_def->d_node = parseExpression();
            // else empty production
            break;
        default:
            return error( t );
        }
        t = d_cur;
    }
    if( t.isErr() )
        error( t );

    if( d_errs )
        return d_errs->getErrCount() == 0;
    else
        return true;
}

EbnfSyntax* EbnfParser::getSyntax()
{
    return d_syn.data();
}

EbnfToken EbnfParser::nextToken()
{
    Q_ASSERT( d_lex != 0 );
    EbnfToken t = d_lex->nextToken();
    while( t.d_type == EbnfToken::Comment )
        t = d_lex->nextToken();
    d_cur = t;
    return t;
}

bool EbnfParser::error(const EbnfToken& t, const QString& msg)
{
    if( d_errs == 0 )
        return false;
    if( t.d_type == EbnfToken::Invalid )
        d_errs->error( EbnfErrors::Syntax, t.d_lineNr, t.d_colNr, t.d_val.toStr() );
    else if( msg.isEmpty() )
        d_errs->error( EbnfErrors::Syntax, t.d_lineNr, t.d_colNr,
                        QString("unexpected symbol '%1'").arg(t.toString().constData()) );
    else
        d_errs->error( EbnfErrors::Syntax, t.d_lineNr, t.d_colNr, msg );
    return false;
}

EbnfSyntax::Node*EbnfParser::parseFactor()
{
    // factor ::= keyword | delimiter | category | "[" expression "]" | "{" expression "}"

    EbnfSyntax::Node* node = 0;

    switch( d_cur.d_type )
    {
    case EbnfToken::Keyword:
    case EbnfToken::Literal:
        node = new EbnfSyntax::Node( EbnfSyntax::Node::Terminal, d_def, d_cur );
        nextToken();
        break;
    case EbnfToken::NonTerm:
        node = new EbnfSyntax::Node( EbnfSyntax::Node::Nonterminal, d_def, d_cur );
        nextToken();
        break;
    case EbnfToken::LBrack:
        {
            nextToken();
            node = parseExpression();
            if( node == 0 )
            {
                return 0;
            }
            if( d_cur.d_type != EbnfToken::RBrack )
            {
                error( d_cur, "expecting ']'" );
                delete node;
                return 0;
            }
            if( !checkCardinality(node) )
                return 0;
            node->d_quant = EbnfSyntax::Node::ZeroOrOne;
            nextToken();
        }
        break;
    case EbnfToken::LPar:
        {
            nextToken();
            node = parseExpression();
            if( node == 0 )
            {
                return 0;
            }
            if( d_cur.d_type != EbnfToken::RPar )
            {
                error( d_cur, "expecting ')'" );
                delete node;
                return 0;
            }
            if( !checkCardinality(node) )
                return 0;
            node->d_quant = EbnfSyntax::Node::One;
            nextToken();
        }
        break;
    case EbnfToken::LBrace:
        {
            nextToken();
            node = parseExpression();
            if( node == 0 )
            {
                return 0;
            }
            if( d_cur.d_type != EbnfToken::RBrace )
            {
                error( d_cur, "expecting '}'" );
                delete node;
                return 0;
            }
            if( !checkCardinality(node) )
                return 0;
            node->d_quant = EbnfSyntax::Node::ZeroOrMore;
            nextToken();
        }
        break;
    default:
        error( d_cur, "expecting keyword, delimiter, category, '{' or '['" );
        return 0;
    }
    return node;
}

EbnfSyntax::Node*EbnfParser::parseExpression()
{
    // expression ::= term { "|" term }

    EbnfSyntax::Node* node = 0;

    const EbnfToken first = d_cur;
    switch( d_cur.d_type )
    {
    case EbnfToken::Keyword:
    case EbnfToken::Literal:
    case EbnfToken::NonTerm:
    case EbnfToken::LBrack:
    case EbnfToken::LPar:
    case EbnfToken::LBrace:
    case EbnfToken::Predicate:
        node = parseTerm();
        break;
    default:
        error( d_cur, "expecting term" );
        return 0;
    }
    if( node == 0 )
        return 0;
    EbnfSyntax::Node* alternative = 0;
    while( d_cur.d_type == EbnfToken::Bar )
    {
        nextToken();
        if( alternative == 0 )
        {
            alternative = new EbnfSyntax::Node( EbnfSyntax::Node::Alternative, d_def );
            alternative->d_tok.d_lineNr = first.d_lineNr;
            alternative->d_tok.d_colNr = first.d_colNr;
            alternative->d_subs.append(node);
            node->d_parent = alternative;
            node = alternative;
        }
        EbnfSyntax::Node* n = parseTerm();
        if( n == 0 )
        {
            delete node;
            return 0;
        }
        alternative->d_subs.append(n);
        n->d_parent = alternative;
    }
    return node;
}

EbnfSyntax::Node*EbnfParser::parseTerm()
{
    // term ::= [ predicate ] factor { factor }

    EbnfSyntax::Node* node = 0;

    EbnfToken pred;
    if( d_cur.d_type == EbnfToken::Predicate )
    {
        pred = d_cur;
        nextToken();
    }

    EbnfToken first = d_cur;
    switch( d_cur.d_type )
    {
    case EbnfToken::Keyword:
    case EbnfToken::Literal:
    case EbnfToken::NonTerm:
    case EbnfToken::LBrack:
    case EbnfToken::LBrace:
    case EbnfToken::LPar:
        node = parseFactor();
        break;
    default:
        error( d_cur, "expecting factor" );
        return 0;
    }
    if( node == 0 )
        return 0;

    EbnfSyntax::Node* sequence = 0;
    if( pred.isValid() )
    {
        sequence = new EbnfSyntax::Node( EbnfSyntax::Node::Sequence, d_def );
        sequence->d_tok.d_lineNr = first.d_lineNr;
        sequence->d_tok.d_colNr = first.d_colNr;
        sequence->d_subs.append(new EbnfSyntax::Node( EbnfSyntax::Node::Predicate, d_def, pred ));
        sequence->d_subs.back()->d_parent = sequence;
        sequence->d_subs.append(node);
        node->d_parent = sequence;
        node = sequence;
    }

    while( d_cur.d_type == EbnfToken::Keyword ||
           d_cur.d_type == EbnfToken::Literal ||
           d_cur.d_type == EbnfToken::NonTerm ||
           d_cur.d_type == EbnfToken::LBrack ||
           d_cur.d_type == EbnfToken::LPar ||
           d_cur.d_type == EbnfToken::LBrace )
    {
        if( sequence == 0 )
        {
            sequence = new EbnfSyntax::Node( EbnfSyntax::Node::Sequence, d_def );
            sequence->d_tok.d_lineNr = node->d_tok.d_lineNr;
            sequence->d_tok.d_colNr = node->d_tok.d_colNr;
            sequence->d_subs.append(node);
            node->d_parent = sequence;
            node = sequence;
        }
        EbnfSyntax::Node* n = parseFactor();
        if( n == 0 )
        {
            delete node;
            return 0;
        }
        sequence->d_subs.append(n);
        n->d_parent = sequence;
    }
    return node;
}

bool EbnfParser::checkCardinality(EbnfSyntax::Node* node)
{
    if( node->d_quant != EbnfSyntax::Node::One )
    {
        error( d_cur, "contradicting nested quantifiers" );
        delete node;
        return false;
    }
    if( node->d_type != EbnfSyntax::Node::Sequence && node->d_type != EbnfSyntax::Node::Alternative )
    {
        Q_ASSERT( node->d_subs.isEmpty() );
        return true;
    }
    if( node->d_subs.isEmpty() )
    {
        error( d_cur, "container with zero items" );
        delete node;
        return false;
    }
    if( node->d_subs.size() == 1 &&
            ( node->d_subs.first()->d_type == EbnfSyntax::Node::Sequence
              || node->d_subs.first()->d_type == EbnfSyntax::Node::Alternative))
    {
        error( d_cur, "container containing only one other sequence or alternative" );
        delete node;
        return false;
    }
    return true;
}

