CREATE SCHEMA xtbatch;

CREATE TABLE xtbatch.eml (
  eml_id serial primary key,
  eml_hash bytea not null UNIQUE,
  eml_date date not null,
  eml_subj text not null,
  eml_body text not null,
  eml_msg bytea not null,
  eml_status char(1) not null CHECK (eml_status IN ('I','O','C'))
);
 
GRANT ALL ON TABLE xtbatch.eml TO xtrole;
GRANT ALL ON SEQUENCE xtbatch.eml_eml_id_seq TO xtrole;
 
COMMENT ON TABLE xtbatch.eml IS 'E-mail Repository';
COMMENT ON COLUMN xtbatch.eml.eml_id IS 'E-mail primary key';
COMMENT ON COLUMN xtbatch.eml.eml_hash IS 'Email md5 hash of message to check for uniqueness';
COMMENT ON COLUMN xtbatch.eml.eml_subj IS 'E-mail subject';
COMMENT ON COLUMN xtbatch.eml.eml_body IS 'E-mail body text';
COMMENT ON COLUMN xtbatch.eml.eml_msg IS 'Complete e-mail message content';
COMMENT ON COLUMN xtbatch.eml.eml_status IS 'Processing status I=In-process, O=open,C=complete';


CREATE TABLE xtbatch.emladdr (
 emladdr_id serial,
 emladdr_eml_id integer not null REFERENCES xtbatch.eml (eml_id),
 emladdr_type text not null CHECK (emladdr_type IN ('FROM','TO','CC','BCC')),
 emladdr_addr text not null,
 emladdr_name text not null
 );
 
 
GRANT ALL ON TABLE xtbatch.emladdr TO xtrole;
GRANT ALL ON SEQUENCE xtbatch.emladdr_emladdr_id_seq TO xtrole;
 
COMMENT ON TABLE xtbatch.emladdr IS 'E-mail addresses';
COMMENT ON COLUMN xtbatch.emladdr.emladdr_id IS 'E-mail address primary key';
COMMENT ON COLUMN xtbatch.emladdr.emladdr_eml_id IS 'Reference to E-mail primary key';
COMMENT ON COLUMN xtbatch.emladdr.emladdr_type IS 'Address type';
COMMENT ON COLUMN xtbatch.emladdr.emladdr_addr IS 'Address';
COMMENT ON COLUMN xtbatch.emladdr.emladdr_name IS 'Addressee name';


CREATE TABLE xtbatch.emlassc (
  emlassc_id serial primary key,
  emlassc_eml_id integer not null references xtbatch.eml (eml_id) ON DELETE CASCADE,
  emlassc_type text not null CHECK (emlassc_type IN ('CRMA','T','C', 'EMP','INCDT','OPP','P','Q','S','TD','TO','V','RA','J')),
  emlassc_assc_id integer );
 
GRANT ALL ON TABLE xtbatch.emlassc TO xtrole;
GRANT ALL ON SEQUENCE xtbatch.emlassc_emlassc_id_seq TO xtrole;
 
COMMENT ON TABLE xtbatch.emlassc IS 'E-mail Associations';
COMMENT ON COLUMN xtbatch.emlassc.emlassc_id IS 'E-mail Association unique identifier';
COMMENT ON COLUMN xtbatch.emlassc.emlassc_eml_id IS 'Reference to E-mail unique identifier';
COMMENT ON COLUMN xtbatch.emlassc.emlassc_type IS 'Association type (i.e. S=Sales Order, C=Customer, T=Contact etc.)';
COMMENT ON COLUMN xtbatch.emlassc.emlassc_assc_id IS 'Reference to associated table key';
