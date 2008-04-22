/* $Id$ */

int
GetBushu(int ind)
{
    return bushu_num[ind];
}

void
print_bushu(char *buf, int radical, int strokes)
{
    int delim;
    
    if (bushu_stroke[radical] > 0) {
	delim = '.';
	strokes -= bushu_stroke[radical];
    } else {
	delim = ':';
    }
    sprintf(buf, "%d%c%d", radical, delim, strokes);
}









