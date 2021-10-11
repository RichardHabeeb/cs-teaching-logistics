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
import multiprocessing

PERM_STAGE_DIR_STR = "stage/"


class Utility:
    def number_of_files_in_path(path):
        files = glob.glob(os.path.join(path, "*"))
        return len(files)

    def latest_file_date_in_path(path):
        files = glob.glob(os.path.join(path, "*"))
        if len(files) == 0:
            return None

        latest_file = max(files, key=os.path.getmtime)
        return os.path.getmtime(latest_file)



class Gradebook:
    NET_ID = 'SIS Login ID'
    NAME = 'Student'

    def __init__(self, csv_file):
        self.ignored_net_ids = [
                "",
                "e507bbac9fdc6d660897785054f8ebe1f9585df8",
                "0be216d426626744a6df3af02b0e498c214850ab"
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
        self.execution = config["execution_phases"]
        self.do_stage = "stage" in config
        if self.do_stage:
            self.stage_template = (None if "template_dir" not in config["stage"]
                    else config["stage"]["template_dir"])
            self.ignore_patterns = ([] if "ignore_patterns" not in config["stage"]
                                        else config["stage"]["ignore_patterns"])

    def get_ignore_patterns_func(self):
            return shutil.ignore_patterns(*(self.ignore_patterns))

    def get_student_due_dates(self, net_id):
        key = (self.extensions[net_id] if net_id in self.extensions else "__default__")
        return self.due_dates[key]

    def get_student_fudged_timestamp(self, net_id, target_date_label):
        dates = self.get_student_due_dates(net_id)
        return (
            datetime.datetime.strptime(dates[target_date_label], Assignment.DATE_FORMAT) +
            datetime.timedelta(minutes=dates["fudge_time_mins"]))

    def get_student_late_percent(self, net_id):
        return self.get_student_due_dates(net_id)["late_percent_per_day"]


    def submitted_before_close(self, net_id, path):
        if not os.path.isdir(path):
            return False

        close = time.mktime(self.get_student_fudged_timestamp(net_id, "close_date").timetuple())
        submitted = Utility.latest_file_date_in_path(path)
        return submitted is None or submitted <= close


    def submitted_before_corrections_close(self, net_id, path):
        if not os.path.isdir(path):
            return False

        close = time.mktime(self.get_student_fudged_timestamp(net_id, "corrections_close").timetuple())
        submitted = Utility.latest_file_date_in_path(path)
        return submitted is None or submitted <= close


    def get_late_deduction(self, net_id, path):
        if not os.path.isdir(path):
            return 0

        due = self.get_student_fudged_timestamp(net_id, "due_date")
        submitted = datetime.datetime.fromtimestamp(Utility.latest_file_date_in_path(path))

        if submitted is None:
            return 0

        return max(0, self.total_points *
                      ((submitted-due).total_seconds()//(24*60*60) + 1) *
                      self.get_student_late_percent(net_id) / 100.0)

    def get_corrections_weight(self, net_id):
        return float(self.get_student_due_dates(net_id)["corrections_weight"])


class TestRunner():

    def __init__(self, assignment, net_id, name, sub_num, dry_run, perm_stage, perm_stage_only, pause, quiet, do_correction_grade):
        self.assignment = assignment
        self.net_id = net_id
        self.name = name
        self.student_path = os.path.join(self.assignment.submit_path, self.net_id)
        self.dry_run = dry_run
        self.perm_stage = perm_stage
        self.perm_stage_only = perm_stage
        self.pause = pause
        self.quiet = quiet
        self.output_log = ""
        self.do_correction_grade = do_correction_grade

        self.latest_submission_path = None
        self.latest_submission_num = None
        if(sub_num is not None):
            self.latest_submission_num = sub_num
            self.latest_submission_path = os.path.join(self.student_path, str(sub_num))
            if not os.path.isdir(self.latest_submission_path):
                raise("\t[!] Submission " + str(sub_num) + " not found.")
        else:
            self.find_latest_valid_submission()

        self.latest_correction_path = None
        self.latest_correction_num = None
        if self.do_correction_grade:
            self.find_latest_valid_correction()

        self.score = 0
        self.extra_credit = 0
        self.correction_bonus = 0
        self.late_penalty = 0

        if self.latest_submission_path is not None:
            self.late_penalty = self.assignment.get_late_deduction(self.net_id, self.latest_submission_path)



    def print(self, s):
        if self.quiet:
            self.output_log += s + "\n"
        else:
            print(s)

    def print_late_info(self):
        print("[i] Student: " + self.name +  " (" + self.net_id + ")...")

        if not os.path.isdir(self.student_path):
            print("\t[!] No submission found.")
            return

        if self.latest_submission_path is None:
            print("\t[!] No on time submissions.")
            return

        print("\t[i] Using:", self.latest_submission_path)
        print("\t[i] Late penalty: " + str(self.late_penalty))


    def grade(self):
        self.print("[i] Grading " + self.name +  " (" + self.net_id + ")...")

        if not os.path.isdir(self.student_path):
            self.print("\t[!] No submission found.")
            return 0

        if self.latest_submission_path is None:
            self.print("\t[!] No on time submissions.")
            return 0

        self.print("\t[i] Using: " + str(self.latest_submission_path))
        self.print("\t[i] Late penalty: " + str(self.late_penalty))

        #create one log file used for both executions
        #each opens in appending mode, so we first delete any previous content
        log_path = os.path.join(self.assignment.output_path,
            self.net_id + "-" + str(self.latest_submission_num) + ".txt")

        if not self.perm_stage_only:
            with open(log_path, "wb") as log:
                log.truncate()

        std_raw_score, std_extra_credit = self.execute(self.latest_submission_path, log_path)


        self.print("\t[i] Raw Score: " + str(std_raw_score))
        self.print("\t[i] Extra Credit: " + str(std_extra_credit))


        cor_raw_score = 0
        cor_extra_credit = 0
        cor_weight = 0
        if self.do_correction_grade and self.latest_correction_path is not None and self.latest_correction_path != self.latest_submission_path:
            self.print("\t[i] Using corrections: " + str(self.latest_correction_path))
            cor_raw_score, cor_extra_credit = self.execute(self.latest_correction_path, log_path)

            self.print("\t[i] Raw Corrections Improvement: " + str(cor_raw_score - std_raw_score))
            self.print("\t[i] Raw Corrections Extra Credit Improvement: " + str(cor_extra_credit - std_extra_credit))

            cor_weight = self.assignment.get_corrections_weight(self.net_id)

        if self.perm_stage_only:
            return None

        self.correction_bonus = max(0, (cor_raw_score - std_raw_score) * cor_weight)
        self.score = max(0, (std_raw_score - self.late_penalty) + self.correction_bonus)
        self.extra_credit = max(0, std_extra_credit + cor_extra_credit * cor_weight)


        self.print("\t[i] Total Score (lateness adjusted, no ec): " + str(self.score))
        self.print("\t[i] Total Extra Credit: " + str(self.extra_credit))
        self.print("\t[i] Log file: " + str(log_path))

        self.write_log_footer(log_path)

        return self.score

    def make_dry_run(self):
        self.print("\t[i] Make dry run (listing files):")
        os.system("ls | sed 's/.*/\t\t&/'")
        self.print("\t[i] Make dry run (make -n):")
        os.system("make -n | sed 's/.*/\t\t&/'")
        result = input("\t[?] Continue? [Y/n]:").lower().strip()
        if result == "n":
            os.chdir(original_working_dir)
            return None

    def setup_perm_stage(self, src_path):
        perm_stage_path = os.path.join(self.student_path, PERM_STAGE_DIR_STR)
        self.print("\t[i] Setting up permanent stage directory in " + str(perm_stage_path))
        shutil.copytree(src_path, perm_stage_path,
                ignore=self.assignment.get_ignore_patterns_func(), dirs_exist_ok=True)

    def setup_stage(self, src_path, exec_path):
        shutil.copytree(src_path, exec_path,
                ignore=self.assignment.get_ignore_patterns_func(), dirs_exist_ok=True)
        # Copy template last, intentionally overwriting any submitted template files
        if self.assignment.stage_template is not None:
            shutil.copytree(self.assignment.stage_template, exec_path, dirs_exist_ok=True)

    def write_log_footer(self, log_path):
        with open(log_path, "a") as f:
            f.write("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n")
            f.write("Late Penalty: " + str(self.late_penalty) + "\n")
            f.write("Total Score (lateness adjusted):" + str(self.score) + "\n")
            f.write("Extra Credit: " + str(self.extra_credit) + "\n")
            f.write("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n")


    def execute(self, src_path, log_path):
        extra_credit_score = 0
        score = 0
        with tempfile.TemporaryDirectory() as temp_dir:
            original_working_dir = os.getcwd()
            exec_path = src_path

            if self.assignment.do_stage:
                exec_path = str(temp_dir)
                self.setup_stage(src_path, exec_path)
                if self.perm_stage:
                    self.setup_perm_stage(src_path)

            if self.perm_stage_only:
                return (None, None)

            os.chdir(exec_path)
            self.print("\t[i] Executing from " + str(exec_path))

            if self.dry_run:
                self.make_dry_run()

            if self.pause is not None and self.pause:
                input("\t[?] Press enter to run commands.")

            self.print("\t[i] Running commands...")
            with open(log_path, "ab") as f:
                for phase in self.assignment.execution:
                    if "extra_credit" in phase and phase["extra_credit"]:
                        extra_credit_score += self.execute_phase(phase, f)
                    else:
                        score += self.execute_phase(phase, f)


            os.chdir(original_working_dir)
        return (score, extra_credit_score)

    def execute_phase(self, phase, log_file):
        log_file.write(b"######################################################################\n")
        self.print("\t\t[i] Phase: " + phase["name"])
        phase_score = 0.0
        for c in phase["cmds"]:
            self.print("\t\t\t[$] " + c)
            command_pipe = subprocess.Popen(shlex.split(c), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            (command_stdout, command_stderr) = command_pipe.communicate()

            log_file.write(b"### STDOUT ##########################\n")
            log_file.write(command_stdout)
            log_file.write(b"### STDERR ##########################\n")
            log_file.write(command_stderr)
            command_score = command_pipe.wait()
            self.print("\t\t\t\t[i] " + str(command_score) + " point(s)")
            phase_score += command_score

        phase_score_scaled = (phase_score / phase["max"])*self.assignment.total_points*phase["weight"]
        self.print("\t\t\t[i] Phase score: " + str(phase_score_scaled))
        return phase_score_scaled


    def find_latest_valid_submission(self):
        sub_num = 0
        curr = os.path.join(self.student_path, str(sub_num))
        while(self.assignment.submitted_before_close(self.net_id, curr)):
            self.latest_submission_path = curr
            self.latest_submission_num = sub_num
            sub_num += 1
            curr = os.path.join(self.student_path, str(sub_num))

    def find_latest_valid_correction(self):
        sub_num = 0
        curr = os.path.join(self.student_path, str(sub_num))
        while(self.assignment.submitted_before_corrections_close(self.net_id, curr)):
            self.latest_correction_path = curr
            self.latest_correction_num = sub_num
            sub_num += 1
            curr = os.path.join(self.student_path, str(sub_num))




def pooled_grader(test_runner):
    test_runner.grade()
    print(test_runner.output_log)
    return test_runner


def main(gradebook_input_file,
         gradebook_output_path,
         assignment_config_file,
         students_to_grade,
         submission_to_grade,
         skip_graded,
         late_info,
         pause_before_running,
         stage_submission,
         stage_submission_only,
         make_dry_run,
         pool_size,
         corrections):
    print("######################################################################")
    print("#                         AUTO GRADER                                #")
    print("######################################################################")

    book = Gradebook(gradebook_input_file)
    print("[i] Found " + str(len(book)) + " students.")

    assignment = Assignment(assignment_config_file)
    original_working_dir = os.getcwd()

    do_pooling = pool_size is not None and pool_size > 1
    do_update_grades = not stage_submission_only and not late_info
    stage_submission = stage_submission or stage_submission_only

    if do_update_grades and (gradebook_output_path is None or len(gradebook_output_path) == 0):
        raise("No gradebook output defined with -o flag")

    runners = []

    student_whitelist = None
    if students_to_grade is not None and len(students_to_grade) > 0:
        student_whitelist = [s.strip() for s in args.students.split(",")]


    for student in book:
        if student_whitelist is not None and student not in student_whitelist:
            continue

        if skip_graded and book.has_grade(student, assignment):
            continue

        if late_info:
            runner.print_late_info()
            continue

        runners.append(TestRunner(assignment, student, book.name(student),
                submission_to_grade, make_dry_run, stage_submission,
                stage_submission_only, pause_before_running, do_pooling, corrections))

    print("[i] Processing " + str(len(runners)) + " students based on selected options.")


    if do_pooling:
        with multiprocessing.Pool(pool_size) as process_pool:
            completed_runners = process_pool.map(pooled_grader, runners)
            process_pool.close()  # stop more tasks from being added
            process_pool.join()  # wait for all workers to complete before moving on

        for completed in completed_runners:
            book.set_grade(completed.net_id, assignment, completed.score)
    else:
        progress = 0
        for runner in runners:
            print("[i] Progress: ", progress * 100.0 / len(runners))
            try:
                runner.grade()
                book.set_grade(runner.net_id, assignment, runner.score)
                progress += 1
            except KeyboardInterrupt:
                os.chdir(original_working_dir)
                break
            finally:
                if do_update_grades:
                    book.write_grades(gradebook_output_path)

    if do_update_grades:
        book.write_grades(gradebook_output_path)

    if stage_submission is not None and stage_submission == True:
        print("""[i] You can use the following line for moss
        moss.pl -l <lang> """
        + ("" if assignment.stage_template is None else "-b " + assignment.stage_template + "/*")
        + " -d "
        + assignment.submit_path
        + "*/stage/*")



if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("gradebook", help="Canvas exported gradebook CSV file")
    parser.add_argument("config", help="Assignment config json file")

    group0 = parser.add_mutually_exclusive_group()
    group0.add_argument("-o", "--output_gradebook", help="Path for where to create the final gradebook")
    group0.add_argument("--stage_submission_only", action="store_true", help="Same as --stage_submission, but no tests are run. No gradebook updates performed.")
    group0.add_argument("--late_info", action="store_true", help="Just show the late deductions. Doesn't output any grades.")


    parser.add_argument("--submission", type=int, help="Grade a specific submission number")
    parser.add_argument("--skip_graded", action="store_true", help="Skip grading students that already have a grade")

    group1 = parser.add_mutually_exclusive_group()
    group1.add_argument("--corrections", action="store_true", help="Run the standard submission and the corrections")

    group2 = parser.add_mutually_exclusive_group()
    group2.add_argument("--make_dry_run", action="store_true", help="Do a make dry run before running the tests (implies --pause)")
    group2.add_argument("--pause", action="store_true", help="Pause before starting to execute user code")
    group2.add_argument("--pool_size", type=int, help="Run multiple students at once (must have non-interactive phases), no intermediate grade-book saving!")

    parser.add_argument("--stage_submission", action="store_true", help="""
            Prepare a staging directory for the latest valid submission under <submit path>/<netid>/stage/
            Can be useful to run plagiarism tester. Not deleted after script finishes""")
    parser.add_argument("--students", type=str, help="Grade a list of netids: \"netID, netID, ...\"")
    #parser.add_argument("--skip", help="Don't grade these students")



    args = parser.parse_args()
    main(
        args.gradebook,
        args.output_gradebook,
        args.config,
        args.students,
        args.submission,
        args.skip_graded,
        args.late_info,
        args.pause,
        args.stage_submission,
        args.stage_submission_only,
        args.make_dry_run,
        args.pool_size,
        args.corrections)
