#include <iostream>
#include <fstream>

#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>
#include <curses.h>

#include <boost/algorithm/string.hpp>

#include <boost/assign/std/vector.hpp>
#include <boost/assign/list_inserter.hpp>

#include <boost/lexical_cast.hpp>

#include <boost/regex.hpp>

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_numeric.hpp>

#include <boost/utility.hpp>

using namespace std;
using namespace boost;
using namespace boost::assign;
using namespace boost::spirit;

typedef map< unsigned short, string > instructions;
typedef map< unsigned short, unsigned short > memory;
typedef map< string, unsigned short > llabels;

unsigned short code_address = 0;

llabels			code_labels;
instructions 	code_instructions;
memory 			compiled;
	
pthread_t keyboard;
bool emulation_running = false;
	
int compile_instruction( string, unsigned short );
void emulation( );

template < typename Result, typename Source >
Result lexical_cast_from_hex( Source value )
{
	std::stringstream convertor;
	convertor << value;
	Result result;
	if (!(convertor >> std::hex >> result) || !convertor.eof())
	{
		throw boost::bad_lexical_cast();
	}
	return result;
}

unsigned short shortval( string s )
{
	if( !istarts_with( s, "0x" ) )
		return lexical_cast< unsigned short >( s . c_str( ) );
	else
		return lexical_cast_from_hex< unsigned short >( s . c_str( ) );
}

int main( int argc, char ** argv )
{
	if( argc != 2 )
	{
		cout << argv[0] << " source.asm\n";
		return 1;
	}
	
	ifstream source( argv[ 1 ] );
	if( !source . is_open( ) )
		return 2;
	
	string line;
	
	regex label_regex( "(\\w+)\\x2C", boost::regex_constants::icase );
	regex org_regex( "ORG\\s+((0x)?\\d+)", boost::regex_constants::icase );
	regex asc_regex( "ASC\\s+\\x27(.+)\\x27", boost::regex_constants::icase );
	regex comment_regex( "\\x2F\\x2F(.+)$", boost::regex_constants::icase );
	
	while( getline( source, line ) )
	{
		cmatch matches;
		
		trim( line );
		if( !line . length( ) ) continue;

		if( regex_search( line . c_str( ), matches, comment_regex ) )
		{
			string l = matches[ 0 ];
			erase_all( line, l );
			trim( line );
		}
		else if( regex_match( line . c_str( ), matches, org_regex ) )
		{
			string org = matches[ 1 ] . first;
			unsigned short _org = shortval( org );

			if( _org > code_address )
				code_address = _org;
				
			continue;
		}
		else if( regex_match( line . c_str( ), matches, asc_regex ) )
		{
			insert( code_instructions )( code_address, line );

			code_address += ceil( matches[ 1 ] . length( ) / 2 );

			if( ( matches[ 1 ] . length( ) % 2 ) == 1 )
			{
				code_address++;
			}
			
			continue;
		}
		else if( regex_search( line . c_str( ), matches, label_regex ) )
		{
			string l = matches[ 1 ];
			erase_all( l, "," );

			insert( code_labels )( l, code_address );
			
			string s = matches[ 1 ];
			
			erase_all( line, s + "," );
			replace_all( line, "\t", " " );
			trim_left_if( line, is_any_of( "\t " ) );

			if( !line . length( ) ) continue;
		}
		
		insert( code_instructions )( code_address, line );
		
		code_address++;
	}

	if( code_instructions . size( ) < 1 )
		return EXIT_SUCCESS;

	BOOST_FOREACH( instructions :: value_type minstruction, code_instructions )
	{
		unsigned short address = minstruction . first;
		string instruction = minstruction . second;

		compile_instruction( instruction, address );
	}
	
	BOOST_FOREACH( memory :: value_type mmemory, compiled )
	{
		printf( "%04x: %04x\n", mmemory . first, mmemory . second );
	}
	
	emulation( );
	
	emulation_running = false;
	//pthread_join( keyboard, NULL );
	
	return EXIT_SUCCESS;
}

void emulation( )
{
	unsigned short AR = 0;
	unsigned short AC = 0;
	unsigned short PC = 0;
	unsigned short IR = 0;
	unsigned short TR = 0;
	unsigned short DR = 0;
	
	bool S = 1;
	bool R = 0;
	bool IEN = 0;
	bool FGI = 0;
	bool FGO = 0;
	bool E = 0;

	emulation_running = true;

	memory M( compiled );
	
	initscr( ); 
	cbreak( ); 
	noecho( ); 
	//keypad( stdscr, TRUE ); 
	nodelay( stdscr, TRUE );
	
	int _keyboard_input = ERR;
	
	while( S )
	{
		usleep( 100 );

		if( _keyboard_input == ERR ) _keyboard_input = wgetch( stdscr );
		if( _keyboard_input != ERR ) { FGI = 1; if( IEN == 1 ) R = 1; }

		if( _keyboard_input == 0x1B ) { S = 0; continue; }

		bool I;
		unsigned short opcode;
		unsigned short data;

		if( !R )
		{
			//	fetch
			AR = PC;
			IR = M[ AR ];
			PC++;
	
			//	decode
			I = ( IR >> 0xF );
			opcode = ( IR >> 0xC ) & ~8;
			data = IR & ~0xF000;

			if( opcode != BOOST_BINARY( 111 ) )
			{
				AR = data;
		
				if( I )
					AR = M[ AR ];
			}
		}
		else if( R && IEN && ( FGI || FGO ) )
		{
			AR = 0;
			TR = PC;
			M[ AR ] = TR;
			PC = 0;
			PC = PC + 1;
			IEN = 0;
			R = 0;
			continue;
		}
		
		if( opcode != BOOST_BINARY( 111 ) )
		{
			switch( opcode )
			{
				case BOOST_BINARY( 000 ): 	//	AND
					AC = AC & M[ AR ];
				break;
				case BOOST_BINARY( 001 ): 	//	ADD
					AC = AC + M[ AR ];
				
					if( AC + M[ AR ] > 0xFFFF )
						E = 1;
				break;
				case BOOST_BINARY( 010 ): 	//	LDA
					AC = M[ AR ];
				break;
				case BOOST_BINARY( 011 ): 	//	STA
					M[ AR ] = AC;
				break;
				case BOOST_BINARY( 100 ): 	//	BUN
					PC = AR;
				break;
				case BOOST_BINARY( 101 ): 	//	BSA
					M[ AR ] = PC;
					AR++;
					PC = AR;
				break;
				case BOOST_BINARY( 110 ): 	//	ISZ
					DR = M[ AR ];
					DR++;
				
					M[ AR ] = DR;
				
					if( DR == 0 )
						PC++;
					
				break;
			}

			continue;
		}
			
		if( I )
		{
			switch( data )
			{
				case BOOST_BINARY( 1000 0000 0000 ):	//	INP
					AC = ( char )_keyboard_input;
					FGI = 0;

					_keyboard_input = ERR;

				break;
				case BOOST_BINARY( 0100 0000 0000 ):	//	OUT
					cout << ( unsigned char )( AC & 0xFF );
					refresh( );
					FGO = 0;
				break;
				case BOOST_BINARY( 0010 0000 0000 ):	//	SKI
					if( FGI == 1 )
						PC++;
				break;
				case BOOST_BINARY( 0001 0000 0000 ):	//	SKO
					if( FGO == 1 )
						PC++;
				break;
				case BOOST_BINARY( 0000 1000 0000 ):	//	ION
					IEN = 1;
				break;
				case BOOST_BINARY( 0000 0100 0000 ):	//	IOF
					IEN = 0;
				break;
			}
		}
		else
		{
			switch( data )
			{
				case BOOST_BINARY( 1000 0000 0000 ):	//	CLA
					AC = 0;
				break;
				case BOOST_BINARY( 0100 0000 0000 ):	//	CLE
					E = 0;
				break;
				case BOOST_BINARY( 0010 0000 0000 ):	//	CMA
					AC = ~AC;
				break;
				case BOOST_BINARY( 0001 0000 0000 ):	//	CME
					E = ~E;
				break;
				case BOOST_BINARY( 0000 1000 0000 ):	//	CIR
					AC = AC >> 1;
				break;
				case BOOST_BINARY( 0000 0100 0000 ):	//	CIL
					AC = AC << 1;
				break;
				case BOOST_BINARY( 0000 0010 0000 ):	//	INC
					AC++;
				break;
				case BOOST_BINARY( 0000 0001 0000 ):	//	SPA
					if( !( AC & 0x8000 ) )
						PC++;
				break;
				case BOOST_BINARY( 0000 0000 1000 ):	//	SNA
					if( AC & 0x8000 )
						PC++;
				break;
				case BOOST_BINARY( 0000 0000 0100 ):	//	SZA
					if( AC == 0 )
						PC++;
				break;
				case BOOST_BINARY( 0000 0000 0010 ):	//	SZE
					if( E == 0 )
						PC++;
				break;
				case BOOST_BINARY( 0000 0000 0001 ):	//	HLT
					S = 0;
				break;
			}	
		}
	}
	
	endwin( );
	
	emulation_running = false;
	
	cout << endl << "Emulation done" << endl;
	
	BOOST_FOREACH( memory :: value_type mmemory, M )
	{
		printf( "%04x: %04x\n", mmemory . first, mmemory . second );
	}
}

#define mk_instruction( I, opcode, data ) ( ( I << 0xF ) | ( opcode << 0xC ) | data )

int compile_instruction( string instruction, unsigned short address )
{
	string lcinstruction = instruction;
	to_lower( lcinstruction );
	
	typedef vector< string > split_vector_type;

	vector< string > tokens;
	split( tokens, lcinstruction, is_any_of( " \t" ), token_compress_on );

	if( tokens . size( ) == 1 )
	{
		if( equals( tokens[ 0 ], "cla" ) )
		{
			insert( compiled )( address, mk_instruction( 0, BOOST_BINARY( 111 ), BOOST_BINARY( 1000 0000 0000 ) ) );
		}
		else if( equals( tokens[ 0 ], "cle" ) )
		{
			insert( compiled )( address, mk_instruction( 0, BOOST_BINARY( 111 ), BOOST_BINARY( 0100 0000 0000 ) ) );
		}
		else if( equals( tokens[ 0 ], "cma" ) )
		{
			insert( compiled )( address, mk_instruction( 0, BOOST_BINARY( 111 ), BOOST_BINARY( 0010 0000 0000 ) ) );
		}
		else if( equals( tokens[ 0 ], "cme" ) )
		{
			insert( compiled )( address, mk_instruction( 0, BOOST_BINARY( 111 ), BOOST_BINARY( 0001 0000 0000 ) ) );
		}
		else if( equals( tokens[ 0 ], "cir" ) )
		{
			insert( compiled )( address, mk_instruction( 0, BOOST_BINARY( 111 ), BOOST_BINARY( 0000 1000 0000 ) ) );
		}
		else if( equals( tokens[ 0 ], "cil" ) )
		{
			insert( compiled )( address, mk_instruction( 0, BOOST_BINARY( 111 ), BOOST_BINARY( 0000 0100 0000 ) ) );
		}
		else if( equals( tokens[ 0 ], "inc" ) )
		{
			insert( compiled )( address, mk_instruction( 0, BOOST_BINARY( 111 ), BOOST_BINARY( 0000 0010 0000 ) ) );
		}
		else if( equals( tokens[ 0 ], "spa" ) )
		{
			insert( compiled )( address, mk_instruction( 0, BOOST_BINARY( 111 ), BOOST_BINARY( 0000 0001 0000 ) ) );
		}
		else if( equals( tokens[ 0 ], "sna" ) )
		{
			insert( compiled )( address, mk_instruction( 0, BOOST_BINARY( 111 ), BOOST_BINARY( 0000 0000 1000 ) ) );
		}
		else if( equals( tokens[ 0 ], "sza" ) )
		{
			insert( compiled )( address, mk_instruction( 0, BOOST_BINARY( 111 ), BOOST_BINARY( 0000 0000 0100 ) ) );
		}
		else if( equals( tokens[ 0 ], "sze" ) )
		{
			insert( compiled )( address, mk_instruction( 0, BOOST_BINARY( 111 ), BOOST_BINARY( 0000 0000 0010 ) ) );
		}
		else if( equals( tokens[ 0 ], "hlt" ) )
		{
			insert( compiled )( address, mk_instruction( 0, BOOST_BINARY( 111 ), BOOST_BINARY( 0000 0000 0001 ) ) );
		}
		
		if( equals( tokens[ 0 ], "inp" ) )
		{
			insert( compiled )( address, mk_instruction( 1, BOOST_BINARY( 111 ), BOOST_BINARY( 1000 0000 0000 ) ) );
		}
		else if( equals( tokens[ 0 ], "out" ) )
		{
			insert( compiled )( address, mk_instruction( 1, BOOST_BINARY( 111 ), BOOST_BINARY( 0100 0000 0000 ) ) );
		}
		else if( equals( tokens[ 0 ], "ski" ) )
		{
			insert( compiled )( address, mk_instruction( 1, BOOST_BINARY( 111 ), BOOST_BINARY( 0010 0000 0000 ) ) );
		}
		else if( equals( tokens[ 0 ], "sko" ) )
		{
			insert( compiled )( address, mk_instruction( 1, BOOST_BINARY( 111 ), BOOST_BINARY( 0001 0000 0000 ) ) );
		}
		else if( equals( tokens[ 0 ], "ion" ) )
		{
			insert( compiled )( address, mk_instruction( 1, BOOST_BINARY( 111 ), BOOST_BINARY( 0000 1000 0000 ) ) );
		}
		else if( equals( tokens[ 0 ], "iof" ) )
		{
			insert( compiled )( address, mk_instruction( 1, BOOST_BINARY( 111 ), BOOST_BINARY( 0000 0100 0000 ) ) );
		}
	}
	else if( tokens . size( ) >= 2 && tokens . size( ) <= 3 )
	{
		bool I = false;
		bool adr_set = false;
		unsigned short adr = 0;

		if( tokens . size( ) == 3 )
		{
			if( tokens[ 2 ][ 0 ] == 'i' )
				I = true;
		}

		if( !equals( tokens[ 0 ], "asc" ) )
		{
			BOOST_FOREACH( llabels :: value_type mlabel, code_labels )
			{
				if( equals( mlabel . first, tokens[ 1 ] ) )
				{
					adr = mlabel . second;
					adr_set = true;
					break;
				}
			}
			
			if( !adr_set )
			{
				try
				{
					adr = shortval( tokens[ 1 ] );
					adr_set = true;
				} catch( void * e )
				{
					adr_set = false;
				}
			}

			if( !adr_set )
				exit( 2 );
		}

		if( equals( tokens[ 0 ], "and" ) )
		{
			insert( compiled )( address, mk_instruction( I, BOOST_BINARY( 000 ), adr ) );
		}
		else if( equals( tokens[ 0 ], "add" ) )
		{
			insert( compiled )( address, mk_instruction( I, BOOST_BINARY( 001 ), adr ) );
		}
		else if( equals( tokens[ 0 ], "lda" ) )
		{
			insert( compiled )( address, mk_instruction( I, BOOST_BINARY( 010 ), adr ) );
		}
		else if( equals( tokens[ 0 ], "sta" ) )
		{
			insert( compiled )( address, mk_instruction( I, BOOST_BINARY( 011 ), adr ) );
		}
		else if( equals( tokens[ 0 ], "bun" ) )
		{
			insert( compiled )( address, mk_instruction( I, BOOST_BINARY( 100 ), adr ) );
		}
		else if( equals( tokens[ 0 ], "bsa" ) )
		{
			insert( compiled )( address, mk_instruction( I, BOOST_BINARY( 101 ), adr ) );
		}
		else if( equals( tokens[ 0 ], "isz" ) )
		{
			insert( compiled )( address, mk_instruction( I, BOOST_BINARY( 110 ), adr ) );
		}
		
		if( equals( tokens[ 0 ], "dec" ) || equals( tokens[ 0 ], "hex" ) )
		{
			insert( compiled )( address, adr );
		}
		else if( equals( tokens[ 0 ], "ptr" ) )
		{
			insert( compiled )( address, adr );
		}
		else if( equals( tokens[ 0 ], "asc" ) )
		{
			split( tokens, instruction, is_any_of( " " ), token_compress_on );
			
			std :: stringstream ss;
			for( size_t i = 1; i < tokens . size( ); i++ )
			{
			  	if( i != 1 )
			    	ss << " ";
			  	ss << tokens[ i ];
			}
			
			string s = ss . str( );
			
			trim_left_if( s, is_any_of( "'" ) );
			trim_right_if( s, is_any_of( "'" ) );

			for( size_t i = 0; i <= s . length( ); i += 2 )
			{
				unsigned short word = ( s[ i ] << 8 );
				if( i + 1 < s . length( ) )
					word = word | s[ i + 1 ];
					
				insert( compiled )( address++, word );
			}
		}
		else if( equals( tokens[ 0 ], "end" ) )
		{
		}
	}
}
