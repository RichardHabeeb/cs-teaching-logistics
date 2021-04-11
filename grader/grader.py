import csv
import argparse
import os
import glob
import time
import datetime
import json
import tempfile
import shutil
import subprocess
import shlex

PERM_STAGE_DIR_STR = "/stage/"

class Gradebook:
    NET_ID = 'SIS Login ID'
    NAME = 'Student'

    def __init__(self, csv_file):
        self.ignored_net_ids = [
                "",
                "e507bbac9fdc6d660897785054f8ebe1f9585df8"
        ]
        self.csv_file = open(csv_file, 'r')
        self.reader = csv.DictReader(self.csv_file)
        self.fieldnames = self.reader.fieldnames
        self.net_id_map = {}
        for row in self.reader:
            net_id = row[Gradebook.NET_ID]
            if net_id not in self.ignored_net_ids:
                self.net_id_map[net_id] = row


    def __iter__(self):
        self.row_iter = iter(self.net_id_map)
        return self

    def __next__(self):
        return next(self.row_iter)

    def __len__(self):
        return len(self.net_id_map)

    def name(self, net_id):
        return self.net_id_map[net_id][Gradebook.NAME]

    def get_grade(self, net_id, assignment):
        return self.net_id_map[net_id][assignment.gradebook_name]

    def set_grade(self, net_id, assignment, score):
        self.net_id_map[net_id][assignment.gradebook_name] = score

    def has_grade(self, net_id, assignment):
        val = self.net_id_map[net_id][assignment.gradebook_name]
        return val is not None and len(str(val)) > 0

    def write_grades(self, output_file):
        with open(output_file, "w+") as out_f:
            writer = csv.DictWriter(out_f, self.fieldnames)
            writer.writeheader()
            for row in self.net_id_map.values():
                writer.writerow(row)

    def __del__(self):
        self.csv_file.close()



class Assignment():
    DATE_FORMAT = "%m/%d/%Y %I:%M %p"

    def __init__(self, config_path):
        with open(config_path) as config_file:
            config = json.load(config_file)

        self.name = config["name"]
        self.gradebook_name = config["canvas_gradebook_name"]
        self.total_points = config["total_points"]
        self.due_dates = config["due_dates"]
        self.extensions = config["extensions"]
        self.submit_path = config["submit_path"]
        self.output_path = config["output_path"]
        self.make_dry_run = config["make_dry_run"]
        self.execution = config["execution_phases"]
        self.do_stage = "stage" in config
        if self.do_stage:
            self.stage_template = (None if "template_dir" not in config["stage"]
                    else config["stage"]["template_dir"])
            self.stage_ignore_patterns = shutil.ignore_patterns(*([] if "ignore_patterns" not in config["stage"]
                else config["stage"]["ignore_patterns"]))

    def submitted_before_close(self, net_id, path):
        if not os.path.isdir(path):
            return False

        key = (self.extensions[net_id] if net_id in self.extensions else "__default__")

        close = time.mktime((
            datetime.datetime.strptime(self.due_dates[key]["close_date"], Assignment.DATE_FORMAT) +
            datetime.timedelta(minutes=self.due_dates[key]["fudge_time_mins"])).timetuple())

        files = glob.glob(os.path.join(path, "*"))
        if len(files) == 0:
            return True

        latest_file = max(files, key=os.path.getmtime)
        t = os.path.getmtime(latest_file)
        return t <= close


    def get_late_deduction(self, net_id, path):
        if not os.path.isdir(path):
            return 0

        key = (self.extensions[net_id] if net_id in self.extensions else "__default__")

        due = (datetime.datetime.strptime(self.due_dates[key]["due_date"], Assignment.DATE_FORMAT) +
              datetime.timedelta(minutes=self.due_dates[key]["fudge_time_mins"]))

        files = glob.glob(os.path.join(path, "*"))
        if len(files) == 0:
            return 0

        latest_file = max(files, key=os.path.getmtime)
        t = datetime.datetime.fromtimestamp(os.path.getmtime(latest_file))

        return max(0, self.total_points *
                      ((t-due).total_seconds()//(24*60*60) + 1) *
                      self.due_dates[key]["late_percent_per_day"] / 100.0)

class TestRunner():

    def __init__(self, assignment, net_id, name, sub_num):
        self.assignment = assignment
        self.net_id = net_id
        self.name = name
        self.student_path = os.path.join(self.assignment.submit_path, self.net_id)
        self.latest_submission_path = None
        self.latest_submission_num = None


    def print_late_info(self):
        print("[i] Student: " + self.name +  " (" + self.net_id + ")...")

        if not os.path.isdir(self.student_path):
            print("\t[!] No submission found.")
            return

        self.find_latest_valid_submission()

        if self.latest_submission_path is None:
            print("\t[!] No on time submissions.")
            return

        print("\t[i] Using:", self.latest_submission_path)

        late_penalty = self.assignment.get_late_deduction(self.net_id, self.latest_submission_path)
        print("\t[i] Late penalty: " + str(late_penalty))


    def grade(self, sub_num=None, pause=False, perm_stage=False):
        print("[i] Grading " + self.name +  " (" + self.net_id + ")...")

        if not os.path.isdir(self.student_path):
            print("\t[!] No submission found.")
            return 0

        if(sub_num is not None):
            self.latest_submission_num = sub_num
            self.latest_submission_path = os.path.join(self.student_path, str(sub_num))
            if not os.path.isdir(self.latest_submission_path):
                print("\t[!] Submission " + sub_num + " not found.")
                return 0
        else:
            self.find_latest_valid_submission()

        if self.latest_submission_path is None:
            print("\t[!] No on time submissions.")
            return 0

        print("\t[i] Using:", self.latest_submission_path)

        late_penalty = self.assignment.get_late_deduction(self.net_id, self.latest_submission_path)
        print("\t[i] Late penalty: " + str(late_penalty))

        score = -late_penalty
        extra_credit_score = 0
        with tempfile.TemporaryDirectory() as temp_dir:
            original_working_dir = os.getcwd()
            exec_path = self.latest_submission_path

            if self.assignment.do_stage:
                exec_path = str(temp_dir)
                self.setup_stage(exec_path)
                if perm_stage:
                    perm_stage_path = self.student_path + PERM_STAGE_DIR_STR
                    print("\t[i] Setting up permanent stage directory in " + perm_stage_path)
                    self.setup_perm_stage(perm_stage_path)

            os.chdir(exec_path)
            print("\t[i] Executing from ", exec_path)

            if self.assignment.make_dry_run:
                self.make_dry_run()

            log_path = os.path.join(self.assignment.output_path,
                self.net_id + "-" + str(self.latest_submission_num) + ".txt")

            if pause is not None and pause:
                input("\t[?] Press enter to run commands.")

            print("\t[i] Running commands...")
            with open(log_path, "wb") as f:
                for phase in self.assignment.execution:
                    if "extra_credit" in phase and phase["extra_credit"]:
                        extra_credit_score += self.execute_phase(phase, f)
                    else:
                        score += self.execute_phase(phase, f)

            print("\t[i] Log file: ", log_path)
            print("\t[i] Standard Score (lateness adjusted):", score)
            print("\t[i] Extra Credit:", extra_credit_score)

            os.chdir(original_working_dir)

        return score

    def make_dry_run(self):
        print("\t[i] Make dry run (listing files):")
        os.system("ls | sed 's/.*/\t\t&/'")
        print("\t[i] Make dry run (make -n):")
        os.system("make -n | sed 's/.*/\t\t&/'")
        result = input("\t[?] Continue? [Y/n]:").lower().strip()
        if result == "n":
            os.chdir(original_working_dir)
            return None

    def setup_perm_stage(self, exec_path):
        shutil.copytree(self.latest_submission_path, exec_path,
                ignore=self.assignment.stage_ignore_patterns, dirs_exist_ok=True)

    def setup_stage(self, exec_path):
        shutil.copytree(self.latest_submission_path, exec_path,
                ignore=self.assignment.stage_ignore_patterns, dirs_exist_ok=True)
        # Copy template last, intentionally overwriting any submitted template files
        if self.assignment.stage_template is not None:
            shutil.copytree(self.assignment.stage_template, exec_path, dirs_exist_ok=True)


    def execute_phase(self, phase, log_file):
        log_file.write(b"######################################################################\n")
        #log_file.write(b"# PHASE: " + phase["name"] + "\n")
        #log_file.write(b"######################################################################\n")
        print("\t\t[i] Phase: " + phase["name"])
        phase_score = 0.0
        for c in phase["cmds"]:
            print("\t\t\t[$] " + c)
            command_pipe = subprocess.Popen(shlex.split(c), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            (command_stdout, command_stderr) = command_pipe.communicate()

            log_file.write(b"### STDOUT ##########################\n")
            log_file.write(command_stdout)
            log_file.write(b"### STDERR ##########################\n")
            log_file.write(command_stderr)
            command_score = command_pipe.wait()
            print("\t\t\t\t[i] " + str(command_score) + " point(s)")
            phase_score += command_score

        phase_score_scaled = (phase_score / phase["max"])*self.assignment.total_points*phase["weight"]
        print("\t\t\t[i] Phase score: " + str(phase_score_scaled))
        return phase_score_scaled


    def find_latest_valid_submission(self):
        sub_num = 0
        curr = os.path.join(self.student_path, str(sub_num))
        while(self.assignment.submitted_before_close(self.net_id, curr)):
            self.latest_submission_path = curr
            self.latest_submission_num = sub_num
            sub_num += 1
            curr = os.path.join(self.student_path, str(sub_num))




def main(gradebook_input_file,
         gradebook_output_path,
         assignment_config_file,
         student_to_grade,
         submission_to_grade,
         skip_graded,
         late_info,
         pause_before_running,
         stage_submission):
    print("######################################################################")
    print("#                         AUTO GRADER                                #")
    print("######################################################################")

    book = Gradebook(gradebook_input_file)
    print("[i] Found " + str(len(book)) + " students.")

    assignment = Assignment(assignment_config_file)
    original_working_dir = os.getcwd()

    progress = 0
    for student in book:
        if student_to_grade is not None and len(student_to_grade) > 0 and student != student_to_grade:
            continue

        print("[i] Grading progress " + str(progress * 100 / len(book)) + "%")
        progress +=1

        if skip_graded and book.has_grade(student, assignment):
            print("[i] Skipping " + student)
            continue

        runner = TestRunner(assignment, student, book.name(student), submission_to_grade)

        if late_info:
            runner.print_late_info()
            continue

        try:
            book.set_grade(student, assignment,
                    runner.grade(submission_to_grade, pause_before_running, stage_submission))
        except KeyboardInterrupt:
            os.chdir(original_working_dir)
            book.write_grades(gradebook_output_path)
            print("")
            break

        print("[i] Updating gradebook (" + gradebook_output_path + ")")
        book.write_grades(gradebook_output_path)




if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("gradebook", help="Canvas exported gradebook CSV file")
    parser.add_argument("config", help="Assignment config json file")
    parser.add_argument("output_gradebook", help="Path for where to create the final gradebook")
    parser.add_argument("--student", type=str, help="Grade a specific student (netid)")
    parser.add_argument("--submission", type=int, help="Grade a specific submission number")
    parser.add_argument("--skip_graded", action="store_true", help="Skip grading students that already have a grade")
    parser.add_argument("--late_info", action="store_true", help="Just show the late deductions")
    parser.add_argument("--pause", action="store_true", help="Pause before starting to execute user code")
    parser.add_argument("--stage_submission", action="store_true", help="""
            Prepare a staging directory for the latest valid submission under <submit path>/<netid>/stage/
            Can be useful to run plagiarism tester. Not deleted after script finishes""")
    #parser.add_argument("--students", type=str, help="Grade a list of netids")
    #parser.add_argument("--skip", help="Don't grade these students")


    args = parser.parse_args()
    main(
        args.gradebook,
        args.output_gradebook,
        args.config,
        args.student,
        args.submission,
        args.skip_graded,
        args.late_info,
        args.pause,
        args.stage_submission)
