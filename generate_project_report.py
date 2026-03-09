import os
from docx import Document

PROJECT_NAME = "Intelligent Agriculture Backend System"

doc = Document()

# ---------- TITLE PAGE ----------
doc.add_heading(PROJECT_NAME, 0)
doc.add_paragraph("Project Documentation")
doc.add_page_break()

# ---------- CONTENTS ----------
doc.add_heading("CONTENTS", level=1)

contents = [
"Declaration",
"Acknowledgement",
"Abstract",
"List Of Figures",
"CHAPTER 1 - SYNOPSIS",
"CHAPTER 2 - SOFTWARE REQUIREMENT AND SPECIFICATION",
"CHAPTER 3 - SYSTEM DESIGN",
"CHAPTER 4 - DATABASE DESIGN",
"CHAPTER 5 - DETAILED DESIGN",
"CHAPTER 6 - CODING",
"CHAPTER 7 - TESTING",
"CHAPTER 8 - USER INTERFACE",
"CHAPTER 9 - USER MANUAL",
"CHAPTER 10 - CONCLUSION",
"CHAPTER 11 - BIBLIOGRAPHY",
"CHAPTER 12 - PLAGIARISM REPORT"
]

for item in contents:
    doc.add_paragraph(item)

doc.add_page_break()

# ---------- CHAPTER 1 ----------
doc.add_heading("CHAPTER 1 - SYNOPSIS", level=1)

doc.add_heading("1.1 Title of the Project", level=2)
doc.add_paragraph(PROJECT_NAME)

doc.add_heading("1.2 Introduction", level=2)
doc.add_paragraph(
"This project develops an intelligent agriculture backend system that "
"integrates soil analysis, crop recommendation, and disease detection "
"using machine learning models."
)

doc.add_heading("1.3 Objectives", level=2)
doc.add_paragraph(
"- Analyze soil data\n"
"- Recommend crops\n"
"- Detect plant diseases\n"
"- Manage agricultural information"
)

doc.add_heading("1.4 Project Category", level=2)
doc.add_paragraph("Web Application with Machine Learning Integration")

doc.add_heading("1.5 Tools/Platforms", level=2)

doc.add_heading("1.5.1 Hardware Requirements", level=3)
doc.add_paragraph(
"Processor: Intel i5 or above\n"
"RAM: 8GB\n"
"Storage: 20GB free disk"
)

doc.add_heading("1.5.2 Software Requirements", level=3)
doc.add_paragraph(
"Operating System: Linux\n"
"Database: SQLite\n"
"Backend: C language\n"
"Frontend: HTML, CSS, JavaScript\n"
"Machine Learning: Python, TensorFlow"
)

doc.add_heading("1.5.3 Tools/Languages Used", level=3)
doc.add_paragraph(
"C, Python, HTML, CSS, JavaScript, SQLite"
)

doc.add_page_break()

# ---------- CHAPTER 2 ----------
doc.add_heading("CHAPTER 2 - SOFTWARE REQUIREMENT SPECIFICATION", level=1)

doc.add_heading("2.1 Introduction", level=2)
doc.add_paragraph(
"This chapter explains the functional and non-functional requirements "
"of the intelligent agriculture system."
)

doc.add_heading("2.2 Overall Description", level=2)

doc.add_heading("2.2.1 Product Perspective", level=3)
doc.add_paragraph(
"The system provides backend services for managing agricultural "
"data and machine learning predictions."
)

doc.add_heading("2.2.2 Product Features", level=3)
doc.add_paragraph(
"- Soil data storage\n"
"- Crop recommendation\n"
"- Disease detection\n"
"- Fertilizer price tracking"
)

doc.add_heading("2.3 Specific Requirements", level=2)

doc.add_heading("2.3.1 User Interface", level=3)
doc.add_paragraph("Web interface for farmers and administrators.")

doc.add_heading("2.3.2 Software Interface", level=3)
doc.add_paragraph("Integration with ML models and SQLite database.")

doc.add_page_break()

# ---------- CHAPTER 3 ----------
doc.add_heading("CHAPTER 3 - SYSTEM DESIGN", level=1)

doc.add_paragraph(
"The system consists of frontend interface, backend server written in C, "
"machine learning modules in Python, and a SQLite database."
)

doc.add_page_break()

# ---------- CHAPTER 4 ----------
doc.add_heading("CHAPTER 4 - DATABASE DESIGN", level=1)

doc.add_paragraph(
"The system uses SQLite databases for storing soil data and user accounts."
)

doc.add_page_break()

# ---------- CHAPTER 5 ----------
doc.add_heading("CHAPTER 5 - DETAILED DESIGN", level=1)

doc.add_paragraph(
"The project is divided into modules including authentication, soil data "
"processing, fertilizer information retrieval, and ML integration."
)

doc.add_page_break()

# ---------- CHAPTER 6 (AUTO LIST CODE FILES) ----------
doc.add_heading("CHAPTER 6 - CODING", level=1)

def list_code_files(folder):
    for root, dirs, files in os.walk(folder):
        for file in files:
            if file.endswith((".c",".h",".py",".js",".html")):
                doc.add_heading(file, level=3)
                doc.add_paragraph("Source code implementation.")

list_code_files(".")

doc.add_page_break()

# ---------- CHAPTER 7 ----------
doc.add_heading("CHAPTER 7 - TESTING", level=1)

doc.add_paragraph(
"Testing was conducted using unit testing and integration testing "
"to ensure system reliability."
)

doc.add_page_break()

# ---------- CHAPTER 8 ----------
doc.add_heading("CHAPTER 8 - USER INTERFACE", level=1)

doc.add_paragraph(
"Frontend pages include login, registration, crop analysis and admin dashboard."
)

doc.add_page_break()

# ---------- CHAPTER 9 ----------
doc.add_heading("CHAPTER 9 - USER MANUAL", level=1)

doc.add_paragraph(
"Users can upload soil data and crop images to receive agricultural recommendations."
)

doc.add_page_break()

# ---------- CHAPTER 10 ----------
doc.add_heading("CHAPTER 10 - CONCLUSION", level=1)

doc.add_paragraph(
"The intelligent agriculture system helps farmers make better decisions "
"using machine learning and data analysis."
)

doc.add_page_break()

# ---------- CHAPTER 11 ----------
doc.add_heading("CHAPTER 11 - BIBLIOGRAPHY", level=1)

doc.add_paragraph(
"TensorFlow Documentation\n"
"SQLite Documentation\n"
"Agriculture Research Papers"
)

doc.add_page_break()

# ---------- CHAPTER 12 ----------
doc.add_heading("CHAPTER 12 - PLAGIARISM REPORT", level=1)
doc.add_paragraph("Attach plagiarism report here.")

doc.save("project_report.docx")

print("Report Generated: project_report.docx")
