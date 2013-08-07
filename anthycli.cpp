#include <QtDebug>
#include <kcmdlineargs.h>
#include <iostream>
#include "Dic.h"
#include "globals.h"

int verb_add(const KCmdLineArgs *args, Dic *d);
int verb_sel(const KCmdLineArgs *args, Dic *d);
int verb_mod(const KCmdLineArgs *args, Dic *d);
int verb_del(const KCmdLineArgs *args, Dic *d);

int main(int argc, char *argv[]) {
    int verb;
    if( argc == 1 ) {
        std::cout << "Too few arguments. Specify the --help option for usage information." << std::endl;
        return 1;
    }
    KCmdLineArgs::init(argc, argv, "anthycli", "", ki18n("anthyCLI"), "0.1");
    KCmdLineOptions opt;
    opt.add("+verb", ki18n("one out of add|mod|del|sel"));
    opt.add("y").add("yomi <yomi>", ki18n("reading of the entry (仮名)"));
    opt.add("s").add("spelling <spelling>", ki18n("spelling of the entry (漢字・仮名)"));
    opt.add("t").add("wordtype <type>", ki18n("Anthy wordtype"));
    opt.add("f").add("frequency <frequency>", ki18n("word frequency (integer)"));
    KCmdLineArgs::addCmdLineOptions(opt);
    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
    switch( (args->arg(0).toLocal8Bit().data())[0] ) {
        case 'm':
            verb = V_MOD;
            break;
        case 'a':
            verb = V_ADD;
            break
        case 'd':
            verb = V_DEL;
            break;
        case 's':
            verb = V_SEL;
            break;
        default:
            qFatal("Unknown verb: %s. Specify the --help option for usage information.", args->arg(0));
            return 1;
    }
    Dic dictionary;

    return 0;
}
