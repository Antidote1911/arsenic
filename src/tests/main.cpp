#define CATCH_CONFIG_RUNNER
#include <QCoreApplication>
#include <QDebug>
#include "catch.hpp"



int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    int result = Catch::Session().run(argc, argv);

        return (result < 0xff ? result : 0xff);
}

int Factorial( int number ) {
   return number <= 1 ? number : Factorial( number - 1 ) * number;  // fail
}



QString upper (QString str)
{
    return str.toUpper();
}

TEST_CASE( "Factorials of 1 and higher are computed (pass)", "[single-file]" ) {
    REQUIRE( Factorial(1) == 1 );
    REQUIRE( Factorial(2) == 2 );
    REQUIRE( Factorial(3) == 6 );
    REQUIRE( Factorial(10) == 3628800 );
}

TEST_CASE( "Upper", "[single-file]" ) {
    REQUIRE( upper("test") == "TEST" );
}

