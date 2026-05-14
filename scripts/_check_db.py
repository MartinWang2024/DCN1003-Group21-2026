import sqlite3
c = sqlite3.connect('cmake-build-debug/programs/Server/data/DCN.db')
print('distinct semesters:')
for r in c.execute("SELECT DISTINCT semester FROM schedules ORDER BY semester"):
    print(' ', r)
print('distinct sections (first 30):')
for r in c.execute("SELECT DISTINCT section FROM courses ORDER BY section LIMIT 30"):
    print(' ', r)
print('one course across semesters:')
for r in c.execute("""SELECT c.code,c.title,c.section,s.semester,s.day,s.duration,s.classroom
                      FROM courses c JOIN schedules s
                        ON c.code=s.course_code AND c.section=s.section
                      WHERE c.code='COMP1023' AND c.section='1001'
                      ORDER BY s.semester, s.day"""):
    print(' ', r)
